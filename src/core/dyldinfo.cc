#include "dyldinfo.hh"
#include "image.hh"
#include "leb.hh"
#include "archive.hh"
#include "section_blob.hh" // Immediate
#include "export_info.hh" // ExportInfo
#include "rebase_info.hh" // RebaseInfo

namespace MachO {

   template <Bits bits>
   DyldInfo<bits>::DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditCommand<bits>(img, offset, env), dyld_info(img.at<dyld_info_command>(offset)),
      rebase(RebaseInfo<bits>::Parse(img, dyld_info.rebase_off, dyld_info.rebase_size, env)),
      bind(BindInfo<bits, false>::Parse(img, dyld_info.bind_off, dyld_info.bind_size, env)),
      lazy_bind(BindInfo<bits, true>::Parse(img, dyld_info.lazy_bind_off, dyld_info.lazy_bind_size,
                                            env)),
      export_info(ExportInfo<bits>::Parse(img, dyld_info.export_off, dyld_info.export_size, env))
   {
      weak_bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld_info.weak_bind_off),
                                       &img.at<uint8_t>(dyld_info.weak_bind_off +
                                                        dyld_info.weak_bind_size));
   }


   template <Bits bits, bool lazy>
   BindInfo<bits, lazy>::BindInfo(const Image& img, std::size_t offset, std::size_t size,
                                  ParseEnv<bits>& env) {
      const std::size_t begin = offset;
      const std::size_t end = begin + size;
      std::size_t it = begin;
      std::size_t vmaddr = 0;
      uint8_t type = 0;
      std::size_t dylib = 0;
      const char *sym = nullptr;
      uint8_t flags = 0;
      std::size_t addend = 0;
      uint32_t index = 0;
               
      while (it != end) {
         const uint8_t byte = img.at<uint8_t>(it);
         const uint8_t opcode = byte & BIND_OPCODE_MASK;
         const uint8_t imm = byte & BIND_IMMEDIATE_MASK;

         ++it;

         std::size_t uleb, uleb2;

         switch (opcode) {
         case BIND_OPCODE_DONE:
         case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
         case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
         case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
         case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
         case BIND_OPCODE_SET_TYPE_IMM:
         case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
         case BIND_OPCODE_DO_BIND:
            break;
            
         case BIND_OPCODE_THREADED:
         case BIND_OPCODE_SET_ADDEND_SLEB:
         case BIND_OPCODE_ADD_ADDR_ULEB:
         case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
         case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
         case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            if constexpr (lazy) {
            throw error("%s: invalid lazy bind opcode 0x%hhx", __FUNCTION__, opcode);
               } else {
               break;
            }
            
         default:
            throw error("%s: invalid bind opcode 0x%x", __FUNCTION__, opcode);
         }

         switch (opcode) {
         case BIND_OPCODE_DONE:
            dylib = 0;
            addend = 0;
            flags = 0;
            type = 0;
            index = it - begin;
            break;
            // return;

         case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
            dylib = imm;
            break;

         case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
            it += leb128_decode(img, it, uleb);
            // it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            dylib = imm;
            break;

         case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
            throw error("%s: BIND_OPCODE_SET_DYLIB_SPECIAL_IMM not supported", __FUNCTION__);

         case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
            flags = imm;
            sym = &img.at<char>(it);
            it += strnlen(sym, end - it) + 1;
            break;

         case BIND_OPCODE_SET_TYPE_IMM:
            type = imm;
            break;

         case BIND_OPCODE_SET_ADDEND_SLEB:
            it += leb128_decode(img, it, addend);
            break;

         case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            it += leb128_decode(img, it, vmaddr);
            vmaddr += env.archive.segment(imm)->segment_command.vmaddr;
            break;

         case BIND_OPCODE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr += (ptr_t) uleb;
            break;

         case BIND_OPCODE_DO_BIND:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags, index);
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags, index);
            it += leb128_decode(img, it, uleb);
            vmaddr += (ptr_t) uleb;
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags, index);
            vmaddr += imm * sizeof(ptr_t);
            break;

         case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            it += leb128_decode(img, it, uleb);
            it += leb128_decode(img, it, uleb2);
            vmaddr = do_bind_times(uleb, vmaddr, env, type, addend, dylib, sym, flags,
                                   (ptr_t) uleb2);
            break;

         case BIND_OPCODE_THREADED:
            throw error("%s: BIND_OPCODE_THREADED not supported", __FUNCTION__);
         }
      }
   }
      
   template <Bits bits, bool lazy>
   std::size_t BindInfo<bits, lazy>::do_bind(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type,
                                             ssize_t addend, std::size_t dylib, const char *sym,
                                             uint8_t flags, uint32_t index) {
      if (vmaddr != 0 && sym != nullptr) {
         bindees.push_back(BindNode<bits, lazy>::Parse(vmaddr, env, type, addend, dylib, sym,
                                                       flags, index));
      }
      return vmaddr + sizeof(ptr_t);
   }

   template <Bits bits, bool lazy>
   std::size_t BindInfo<bits, lazy>::do_bind_times(std::size_t count, std::size_t vmaddr,
                                                   ParseEnv<bits>& env, uint8_t type,
                                                   ssize_t addend, std::size_t dylib,
                                                   const char *sym, uint8_t flags, uint32_t index, 
                                                   ptr_t skipping) {
      for (std::size_t i = 0; i < count; ++i) {
         vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags, index);
         vmaddr += skipping;
      }
      return vmaddr;
   }

   template <Bits bits, bool lazy>
   BindNode<bits, lazy>::BindNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type,
                                  ssize_t addend, std::size_t dylib, const char *sym, uint8_t flags,
                                  uint32_t index):
      type(type), addend(addend), dylib(nullptr), sym(sym), flags(flags), blob(nullptr),
      index(index)
   {
      env.vmaddr_resolver.resolve(vmaddr, &blob);
      env.dylib_resolver.resolve(dylib, &this->dylib);
      if constexpr (lazy) {
            env.lazy_bind_node_resolver.add(index, this);
         }
   }

   template <Bits bits>
   void DyldInfo<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
      dyld_info.rebase_size = rebase->size();
      dyld_info.rebase_off = env.allocate(dyld_info.rebase_size);

      dyld_info.bind_size = bind->size();
      dyld_info.bind_off = env.allocate(dyld_info.bind_size);
      bind->Build(env);

      dyld_info.weak_bind_size = align<bits>(weak_bind.size());
      dyld_info.weak_bind_off = env.allocate(dyld_info.weak_bind_size);
      
      dyld_info.lazy_bind_size = align<bits>(lazy_bind->size());
      dyld_info.lazy_bind_off = env.allocate(dyld_info.lazy_bind_size);
      lazy_bind->Build(env);

      dyld_info.export_size = align<bits>(export_info->size());
      dyld_info.export_off = env.allocate(dyld_info.export_size);
   }

   template <Bits bits, bool lazy>
   std::size_t BindInfo<bits, lazy>::size() const {
      std::size_t size = 0;
      for (const BindNode<bits, lazy> *node : bindees) {
         size += node->size();
      }
      if (size > 0) {
         ++size;
      }
      return align<bits>(size);
   }

   template <Bits bits, bool lazy>
   std::size_t BindNode<bits, lazy>::size() const {
      if (!active()) { return 0; }

      if (!lazy) {
         /* NON-LAZY
          * 1   BIND_OPCODE_SET_DYLIB_ORDINAL_IMM 
          * 1   BIND_OPCODE_SET_TYPE_IMM
          * 1+a   [BIND_OPCODE_SET_ADDEND_SLEB]
          * 1+b BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
          * 1+c BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
          * 1   BIND_OPCODE_DO_BIND
          * 6+a+b+c total
          */
         return
            1 +
            1 +
            (1 + leb128_size(addend)) +
            (1 + leb128_size(blob->loc.offset - blob->segment->loc().offset)) +
            (1 + (sym.size() + 1)) +
            1;
      } else {
         /* LAZY 
          * 1+a BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
          * 1   BIND_OPCODE_SET_DYLIB_ORDINAL_IMM
          * 1+c BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
          * 1   BIND_OPCODE_DO_BIND
          * 1   BIND_OPCODE_DONE
          */
         return
            (1 + leb128_size(blob->loc.offset - blob->segment->loc().offset)) +
            1 +
            (1 + (sym.size() + 1)) +
            1 +
            1;
      }
   }

   template <Bits bits>
   void DyldInfo<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dyld_info_command>(offset) = dyld_info;

      rebase->Emit(img, dyld_info.rebase_off);
      bind->Emit(img, dyld_info.bind_off);

      img.copy(dyld_info.weak_bind_off, &*weak_bind.begin(), dyld_info.weak_bind_size);

      lazy_bind->Emit(img, dyld_info.lazy_bind_off);
      export_info->Emit(img, dyld_info.export_off);
   }

   template <Bits bits, bool lazy>
   void BindInfo<bits, lazy>::Emit(Image& img, std::size_t offset) const {
      for (BindNode<bits, lazy> *bindee : bindees) {
         bindee->Emit(img, offset);
         offset += bindee->size();
      }
      if (!bindees.empty()) {
         img.at<uint8_t>(offset) = BIND_OPCODE_DONE;
      }
   }

   template <Bits bits, bool lazy>
   void BindNode<bits, lazy>::Emit(Image& img, std::size_t offset) const {
      if (!active()) {
         return;
      }

      if (!lazy) {
         /* BIND_OPCODE_SET_DYLIB_ORDINAL_IMM 
          * BIND_OPCODE_SET_TYPE_IMM
          * BIND_OPCODE_SET_ADDEND_SLEB
          * BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
          * BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
          * BIND_OPCODE_DO_BIND
          */
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_DYLIB_ORDINAL_IMM | dylib->id;
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_TYPE_IMM | type;
         
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_ADDEND_SLEB;
         offset += leb128_encode(img, offset, addend);
         
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
         const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
         offset += leb128_encode(img, offset, segoff);
         
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | flags;
         img.copy(offset, sym.c_str(), sym.size() + 1);
         offset += sym.size() + 1;
         img.at<uint8_t>(offset++) = BIND_OPCODE_DO_BIND;
      } else {
         /* LAZY 
          * BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
          * BIND_OPCODE_SET_DYLIB_ORDINAL_IMM
          * BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
          * BIND_OPCODE_DO_BIND
          * BIND_OPCODE_DONE
          */
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
         const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
         offset += leb128_encode(img, offset, segoff);

         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_DYLIB_ORDINAL_IMM | dylib->id;
         
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | flags;
         img.copy(offset, sym.c_str(), sym.size() + 1);
         offset += sym.size() + 1;

         img.at<uint8_t>(offset++) = BIND_OPCODE_DO_BIND;
         img.at<uint8_t>(offset++) = BIND_OPCODE_DONE;
      }
   }

   template <Bits bits>
   std::size_t DyldInfo<bits>::content_size() const {
      return rebase->size() + bind->size() +
         weak_bind.size() + lazy_bind->size() + export_info->size();
   }

   template <Bits bits, bool lazy>
   BindInfo<bits, lazy>::BindInfo(const BindInfo<opposite<bits>, lazy>& other,
                            TransformEnv<opposite<bits>>& env) {
      for (const auto other_bindee : other.bindees) {
         bindees.push_back(other_bindee->Transform(env));
      }
   }

   template <Bits bits, bool lazy>
   BindNode<bits, lazy>::BindNode(const BindNode<opposite<bits>, lazy>& other,
                            TransformEnv<opposite<bits>>& env):
      type(other.type), addend(other.addend), dylib(nullptr), sym(other.sym), flags(other.flags),
      blob(nullptr)
   {
      if constexpr (lazy) { env.template add<LazyBindNode>(&other, this); }
      env.resolve(other.dylib, &dylib);
      env.resolve(other.blob, &blob);
   }

   template <Bits bits, bool lazy>
   void BindInfo<bits, lazy>::print(std::ostream& os) const {
      os << "segment section address dylib symbol" << std::endl;
      for (auto bindee : bindees) {
         bindee->print(os);
         os << std::endl;
      }
   }
   
   template <Bits bits, bool lazy>
   void BindNode<bits, lazy>::print(std::ostream& os) const {
      os << blob->segment->name() << " " << blob->section->name() << " 0x" << std::hex
         << blob->loc.vmaddr << " " << dylib->name << " " << sym;
   }

   template <Bits bits, bool lazy>
   void BindNode<bits, lazy>::Build(BuildEnv<bits>& env) {
      if constexpr (lazy) {
            index = env.lazy_bind_index(size());
         } else {
         index = 0;
      }
   }

   template <Bits bits, bool lazy>
   void BindInfo<bits, lazy>::Build(BuildEnv<bits>& env) {
      for (auto bindee : bindees) {
         bindee->Build(env);
      }
   }

   template class DyldInfo<Bits::M32>;
   template class DyldInfo<Bits::M64>;

   template class BindInfo<Bits::M32, true>;
   template class BindInfo<Bits::M32, false>;
   template class BindInfo<Bits::M64, true>;
   template class BindInfo<Bits::M64, false>;

}
