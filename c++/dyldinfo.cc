#include "dyldinfo.hh"
#include "image.hh"
#include "leb.hh"
#include "macho.hh"

namespace MachO {

   template <Bits bits>
   DyldInfo<bits>::DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      dyld_info(img.at<dyld_info_command>(offset)),
      rebase(RebaseInfo<bits>::Parse(img, dyld_info.rebase_off, dyld_info.rebase_size, env)),
      bind(BindInfo<bits>::Parse(img, dyld_info.bind_off, dyld_info.bind_size, env))
   {
      weak_bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld_info.weak_bind_off),
                                       &img.at<uint8_t>(dyld_info.weak_bind_off +
                                                        dyld_info.weak_bind_size));
      lazy_bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld_info.lazy_bind_off),
                                       &img.at<uint8_t>(dyld_info.lazy_bind_off +
                                                        dyld_info.lazy_bind_size));
      export_info = std::vector<uint8_t>(&img.at<uint8_t>(dyld_info.export_off),
                                         &img.at<uint8_t>(dyld_info.export_off +
                                                          dyld_info.export_size));
   }

   template <Bits bits>
   RebaseInfo<bits>::RebaseInfo(const Image& img, std::size_t offset, std::size_t size,
                                ParseEnv<bits>& env) {
      const std::size_t begin = offset;
      const std::size_t end = begin + size;
      std::size_t it = begin;
      uint8_t type = 0;
      std::size_t vmaddr = 0;
      while (it != end) {
         const uint8_t byte = img.at<uint8_t>(it);
         const uint8_t opcode = byte & REBASE_OPCODE_MASK;
         const uint8_t imm = byte & REBASE_IMMEDIATE_MASK;
         ++it;

         std::size_t uleb, uleb2;
         
         switch (opcode) {
         case REBASE_OPCODE_DONE:
            return;

         case REBASE_OPCODE_SET_TYPE_IMM:
            type = imm;
            break;

         case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, vmaddr);
            vmaddr += env.archive.segment(imm)->vmaddr();
            break;

         case REBASE_OPCODE_ADD_ADDR_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr += uleb;
            break;

         case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
            vmaddr += imm * sizeof(ptr_t);
            break;

         case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
            vmaddr = do_rebase_times(imm, vmaddr, env, type);
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr = do_rebase_times(imm, vmaddr, env, type);
            break;

         case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr = do_rebase(vmaddr, env, type);
            vmaddr += uleb;
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb2);
            vmaddr = do_rebase_times(uleb, vmaddr, env, type, uleb2);
            break;
            
         default:
            throw error("%s: invalid rebase opcode", __FUNCTION__);
         }
         
      }
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type) {
      rebasees.push_back(RebaseNode<bits>::Parse(vmaddr, env, type));
      return vmaddr + sizeof(ptr_t);
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase_times(std::size_t count, std::size_t vmaddr,
                                                 ParseEnv<bits>& env, uint8_t type,
                                                 std::size_t skipping) {
      for (std::size_t i = 0; i < count; ++i) {
         do_rebase(vmaddr, env, type);
         vmaddr += sizeof(ptr_t) + skipping;
      }
      return vmaddr;
   }

   template <Bits bits>
   BindInfo<bits>::BindInfo(const Image& img, std::size_t offset, std::size_t size,
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
               
      while (it != end) {
         const uint8_t byte = img.at<uint8_t>(it);
         const uint8_t opcode = byte & BIND_OPCODE_MASK;
         const uint8_t imm = byte & BIND_IMMEDIATE_MASK;

         ++it;

         std::size_t uleb, uleb2;

         switch (opcode) {
         case BIND_OPCODE_DONE:
            fprintf(stderr, "BIND_OPCODE_DONE\n");
            return;

         case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
            fprintf(stderr, "BIND_OPCODE_SET_DYLIB_ORDINAL_IMM\n");
            dylib = imm;
            break;

         case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
            fprintf(stderr, "BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB\n");
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            dylib = imm;
            break;

         case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
            fprintf(stderr, "BIND_OPCODE_SET_DYLIB_SPECIAL_IMM\n");
            throw error("%s: BIND_OPCODE_SET_DYLIB_SPECIAL_IMM not supported", __FUNCTION__);

         case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
            fprintf(stderr, "BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM\n");
            flags = imm;
            sym = &img.at<char>(it);
            it += strnlen(sym, end - it) + 1;
            break;

         case BIND_OPCODE_SET_TYPE_IMM:
            fprintf(stderr, "BIND_OPCODE_SET_TYPE_IMM\n");
            type = imm;
            break;

         case BIND_OPCODE_SET_ADDEND_SLEB:
            fprintf(stderr, "BIND_OPCODE_SET_ADDEND_SLEB\n");
            it += leb128_decode(&img.at<uint8_t>(it), end - it, addend);
            break;

         case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            fprintf(stderr, "BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB\n");
            it += leb128_decode(&img.at<uint8_t>(it), end - it, vmaddr);
            vmaddr += env.archive.segment(imm)->segment_command.vmaddr;
            break;

         case BIND_OPCODE_ADD_ADDR_ULEB:
            fprintf(stderr, "BIND_OPCODE_ADD_ADDR_ULEB\n");
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            fprintf(stderr, "uleb=%zx\n", uleb);
            vmaddr += (ptr_t) uleb;
            break;

         case BIND_OPCODE_DO_BIND:
            fprintf(stderr, "BIND_OPCODE_DO_BINDn");
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags);
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
            fprintf(stderr, "BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB\n");
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags);
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr += (ptr_t) uleb;
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
            fprintf(stderr, "BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED\n");
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags);
            vmaddr += imm * sizeof(ptr_t);
            break;

         case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            fprintf(stderr, "BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB\n");
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb2);
            vmaddr = do_bind_times(uleb, vmaddr, env, type, addend, dylib, sym, flags,
                                   (ptr_t) uleb2);
            break;

         case BIND_OPCODE_THREADED:
            throw error("%s: BIND_OPCODE_THREADED not supported", __FUNCTION__);

         default:
            throw error("%s: invalid bind opcode %d", __FUNCTION__, opcode);
         }
      }
      
         
   }
      
   template <Bits bits>
   std::size_t BindInfo<bits>::do_bind(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type,
                                       ssize_t addend, std::size_t dylib, const char *sym,
                                       uint8_t flags) {
      fprintf(stderr, "%s: binding at 0x%zx\n", __FUNCTION__, vmaddr);
      bindees.push_back(BindNode<bits>::Parse(vmaddr, env, type, addend, dylib, sym, flags));
      return vmaddr + sizeof(ptr_t);
   }

   template <Bits bits>
   std::size_t BindInfo<bits>::do_bind_times(std::size_t count, std::size_t vmaddr,
                                             ParseEnv<bits>& env, uint8_t type, ssize_t addend,
                                             std::size_t dylib, const char *sym, uint8_t flags,
                                             ptr_t skipping) {
      for (std::size_t i = 0; i < count; ++i) {
         vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags);
         vmaddr += skipping;
      }
      return vmaddr;
   }

   template <Bits bits>
   BindNode<bits>::BindNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend,
                            std::size_t dylib, const char *sym, uint8_t flags):
      type(type), addend(addend), dylib(nullptr), sym(sym), flags(flags), blob(nullptr)
   {
      env.vmaddr_resolver.resolve(vmaddr, &blob);
      env.dylib_resolver.resolve(dylib, &this->dylib);
   }

   template <Bits bits>
   void DyldInfo<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
      dyld_info.rebase_size = rebase->size();
      dyld_info.rebase_off = env.allocate(dyld_info.rebase_size);

      dyld_info.bind_size = bind->size();
      dyld_info.bind_off = env.allocate(dyld_info.bind_size);

      dyld_info.weak_bind_size = weak_bind.size();
      dyld_info.weak_bind_off = env.allocate(dyld_info.weak_bind_size);

      dyld_info.lazy_bind_size = lazy_bind.size();
      dyld_info.lazy_bind_off = env.allocate(dyld_info.lazy_bind_size);

      dyld_info.export_size = export_info.size();
      dyld_info.export_off = env.allocate(dyld_info.export_size);
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::size() const {
      std::size_t size = 0;
      for (const RebaseNode<bits> *node : rebasees) {
         size += node->size();
      }
      size += 1; /* REBASE_OPCODE_DONE */
      return size;
   }

   template <Bits bits>
   std::size_t BindInfo<bits>::size() const {
      std::size_t size = 0;
      for (const BindNode<bits> *node : bindees) {
         size += node->size();
      }
      size += 1; /* BIND_OPCODE_DONE */
      return size;
   }

   template <Bits bits>
   std::size_t BindNode<bits>::size() const {
      /* 1   BIND_OPCODE_SET_DYLIB_ORDINAL_IMM 
       * 1   BIND_OPCODE_SET_TYPE_IMM
       * 1+a   [BIND_OPCODE_SET_ADDEND_SLEB]
       * 1+b BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * 1+c BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
       * 1   BIND_OPCODE_DO_BIND
       * 6+a+b+c total
       */
      assert(dylib->id < 16);
      return 6 + leb128_size(addend) + leb128_size(blob->loc.offset - blob->segment->loc().offset)
         + sym.size() + 1;
   }

   template <Bits bits>
   RebaseNode<bits>::RebaseNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type):
      type(type), blob(nullptr)
   {
      env.vmaddr_resolver.resolve(vmaddr, &blob);
   }

   template <Bits bits>
   std::size_t RebaseNode<bits>::size() const {
      /* 1   REBASE_OPCODE_SET_TYPE_IMM
       * 1+a REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * 1   REBASE_OPCODE_DO_REBASE_IMM_TIMES
       * 3+a total
       */
      assert(blob->segment->id < 16);
      return 3 + leb128_size(blob->loc.offset - blob->segment->loc().offset);
   }

   template <Bits bits>
   void DyldInfo<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dyld_info_command>(offset) = dyld_info;

      rebase->Emit(img, dyld_info.rebase_off);
      bind->Emit(img, dyld_info.bind_off);

      memcpy(&img.at<uint8_t>(dyld_info.weak_bind_off), &*weak_bind.begin(),
             dyld_info.weak_bind_size);
      memcpy(&img.at<uint8_t>(dyld_info.lazy_bind_off), &*lazy_bind.begin(),
             dyld_info.lazy_bind_size);
      memcpy(&img.at<uint8_t>(dyld_info.export_off), &*export_info.begin(),
             dyld_info.export_size);
   }

   template <Bits bits>
   void RebaseInfo<bits>::Emit(Image& img, std::size_t offset) const {
      for (RebaseNode<bits> *rebasee : rebasees) {
         rebasee->Emit(img, offset);
         offset += rebasee->size();
      }
      img.at<uint8_t>(offset) = REBASE_OPCODE_DONE;
   }

   template <Bits bits>
   void RebaseNode<bits>::Emit(Image& img, std::size_t offset) const {
      /* REBASE_OPCODE_SET_TYPE_IMM
       * REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * REBASE_OPCODE_DO_REBASE_IMM_TIMES
       */
      img.at<uint8_t>(offset++) = REBASE_OPCODE_SET_TYPE_IMM | type;
      img.at<uint8_t>(offset++) = REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
      const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
      offset += leb128_encode(&img.at<uint8_t>(offset), img.size() - offset, segoff);
      img.at<uint8_t>(offset++) = REBASE_OPCODE_DO_REBASE_IMM_TIMES | 0x1;
   }

   template <Bits bits>
   void BindInfo<bits>::Emit(Image& img, std::size_t offset) const {
      for (BindNode<bits> *bindee : bindees) {
         bindee->Emit(img, offset);
         offset += bindee->size();
      }
      img.at<uint8_t>(offset) = BIND_OPCODE_DONE;
   }

   template <Bits bits>
   void BindNode<bits>::Emit(Image& img, std::size_t offset) const {
      /* BIND_OPCODE_SET_DYLIB_ORDINAL_IMM 
       * BIND_OPCODE_SET_TYPE_IMM
       * BIND_OPCODE_SET_ADDEND_SLEB
       * BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
       * BIND_OPCODE_DO_BIND
       */
      assert(dylib->id < 16);
      img.at<uint8_t>(offset++) = BIND_OPCODE_SET_DYLIB_ORDINAL_IMM | dylib->id;
      img.at<uint8_t>(offset++) = BIND_OPCODE_SET_TYPE_IMM | type;
      img.at<uint8_t>(offset++) = BIND_OPCODE_SET_ADDEND_SLEB;
      offset += leb128_encode(&img.at<uint8_t>(offset), img.size() - offset, addend);
      img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
      const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
      offset += leb128_encode(&img.at<uint8_t>(offset), img.size() - offset, segoff);
      img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | flags;
      memcpy(&img.at<char>(offset), sym.c_str(), sym.size() + 1); // +1 for NUL
      offset += sym.size() + 1;
      img.at<uint8_t>(offset++) = BIND_OPCODE_DO_BIND;
   }

   template <Bits bits>
   std::size_t DyldInfo<bits>::content_size() const {
      return rebase->size() + bind->size() +
         weak_bind.size() + lazy_bind.size() + export_info.size();
   }
   
   template class DyldInfo<Bits::M32>;
   template class DyldInfo<Bits::M64>;

}
