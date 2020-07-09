#include "dyldinfo.hh"
#include "image.hh"
#include "leb.hh"
#include "archive.hh"
#include "section_blob.hh" // Immediate

namespace MachO {

   template <Bits bits>
   DyldInfo<bits>::DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditCommand<bits>(img, offset, env), dyld_info(img.at<dyld_info_command>(offset)),
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
            it += leb128_decode(img, it, vmaddr);
            vmaddr += env.archive.segment(imm)->vmaddr();
            break;

         case REBASE_OPCODE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr += uleb;
            break;

         case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
            vmaddr += imm * sizeof(ptr_t);
            break;

         case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
            vmaddr = do_rebase_times(imm, vmaddr, env, type);
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
            it += leb128_decode(img, it, uleb);
            vmaddr = do_rebase_times(uleb, vmaddr, env, type);
            break;

         case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr = do_rebase(vmaddr, env, type);
            vmaddr += uleb;
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
            it += leb128_decode(img, it, uleb);
            it += leb128_decode(img, it, uleb2);
            vmaddr = do_rebase_times(uleb, vmaddr, env, type, uleb2);
            break;
            
         default:
            throw error("%s: invalid rebase opcode", __FUNCTION__);
         }
         
      }
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type) {
      std::cerr << "[PARSE] REBASE @ 0x" << std::hex << vmaddr << std::endl;
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
            return;

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
            // it += leb128_decode(&img.at<uint8_t>(it), end - it, addend);
            break;

         case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            it += leb128_decode(img, it, vmaddr);
            // it += leb128_decode(&img.at<uint8_t>(it), end - it, vmaddr);
            vmaddr += env.archive.segment(imm)->segment_command.vmaddr;
            break;

         case BIND_OPCODE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr += (ptr_t) uleb;
            break;

         case BIND_OPCODE_DO_BIND:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags);
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags);
            it += leb128_decode(img, it, uleb);
            // it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr += (ptr_t) uleb;
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags);
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

         default:
            throw error("%s: invalid bind opcode %d", __FUNCTION__, opcode);
         }
      }
      
         
   }
      
   template <Bits bits>
   std::size_t BindInfo<bits>::do_bind(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type,
                                       ssize_t addend, std::size_t dylib, const char *sym,
                                       uint8_t flags) {
      if (vmaddr != 0 && sym != nullptr) {
         bindees.push_back(BindNode<bits>::Parse(vmaddr, env, type, addend, dylib, sym, flags));
      }
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

      dyld_info.weak_bind_size = align<bits>(weak_bind.size());
      dyld_info.weak_bind_off = env.allocate(dyld_info.weak_bind_size);
      
      dyld_info.lazy_bind_size = align<bits>(lazy_bind.size());
      dyld_info.lazy_bind_off = env.allocate(dyld_info.lazy_bind_size);

      dyld_info.export_size = align<bits>(export_info.size());
      dyld_info.export_off = env.allocate(dyld_info.export_size);
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::size() const {
      std::size_t size = 0;
      for (const RebaseNode<bits> *node : rebasees) {
         size += node->size();
      }
      if (size > 0) {
         ++size; /* REBASE_OPCODE_DONE */
      }
      return align<bits>(size);
   }

   template <Bits bits>
   std::size_t BindInfo<bits>::size() const {
      std::size_t size = 0;
      for (const BindNode<bits> *node : bindees) {
         size += node->size();
      }
      if (size > 0) {
         ++size;
      }
      return align<bits>(size);
   }

   template <Bits bits>
   std::size_t BindNode<bits>::size() const {
      if (!active()) { return 0; }
      
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
      struct callback: decltype(env.vmaddr_resolver)::functor {
         ParseEnv<bits>& env;

         callback(ParseEnv<bits>& env): env(env) {}
         
         virtual void operator()(SectionBlob<bits> *blob) override {
            auto imm = dynamic_cast<Immediate<bits> *>(blob);
            if (imm) {
               imm->pointee = env.add_placeholder(imm->value);
            }
         }
      };
      
      env.vmaddr_resolver.resolve(vmaddr, &blob, std::make_shared<callback>(env));
   }

   template <Bits bits>
   std::size_t RebaseNode<bits>::size() const {
      if (!active()) {
         return 0;
      }
      
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

      img.copy(dyld_info.weak_bind_off, &*weak_bind.begin(), dyld_info.weak_bind_size);
      img.copy(dyld_info.lazy_bind_off, &*lazy_bind.begin(), dyld_info.lazy_bind_size);

#warning TODO -- emit ExportInfo
   }

   template <Bits bits>
   void RebaseInfo<bits>::Emit(Image& img, std::size_t offset) const {
      for (RebaseNode<bits> *rebasee : rebasees) {
         rebasee->Emit(img, offset);
         offset += rebasee->size();
      }
      if (!rebasees.empty()) {
         img.at<uint8_t>(offset) = REBASE_OPCODE_DONE;
      }
   }

   template <Bits bits>
   void RebaseNode<bits>::Emit(Image& img, std::size_t offset) const {
      if (!active()) {
         return;
      }
      
      /* REBASE_OPCODE_SET_TYPE_IMM
       * REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * REBASE_OPCODE_DO_REBASE_IMM_TIMES
       */
      img.at<uint8_t>(offset++) = REBASE_OPCODE_SET_TYPE_IMM | type;
      img.at<uint8_t>(offset++) = REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
      const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
      offset += leb128_encode(img, offset, segoff);
      // offset += leb128_encode(&img.at<uint8_t>(offset), img.size() - offset, segoff);
      img.at<uint8_t>(offset++) = REBASE_OPCODE_DO_REBASE_IMM_TIMES | 0x1;
   }

   template <Bits bits>
   void BindInfo<bits>::Emit(Image& img, std::size_t offset) const {
      for (BindNode<bits> *bindee : bindees) {
         bindee->Emit(img, offset);
         offset += bindee->size();
      }
      if (!bindees.empty()) {
         img.at<uint8_t>(offset) = BIND_OPCODE_DONE;
      }
   }

   template <Bits bits>
   void BindNode<bits>::Emit(Image& img, std::size_t offset) const {
      if (!active()) {
         return;
      }
      
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
      offset += leb128_encode(img, offset, addend);
      // offset += leb128_encode(&img.at<uint8_t>(offset), img.size() - offset, addend);
      img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
      const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
      offset += leb128_encode(img, offset, segoff);
      // offset += leb128_encode(&img.at<uint8_t>(offset), img.size() - offset, segoff);
      img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | flags;
      img.copy(offset, sym.c_str(), sym.size() + 1);
      offset += sym.size() + 1;
      img.at<uint8_t>(offset++) = BIND_OPCODE_DO_BIND;
   }

   template <Bits bits>
   std::size_t DyldInfo<bits>::content_size() const {
      return rebase->size() + bind->size() +
         weak_bind.size() + lazy_bind.size() + export_info.size();
   }

   template <Bits bits>
   RebaseInfo<bits>::RebaseInfo(const RebaseInfo<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env) {
      for (const auto other_rebasee : other.rebasees) {
         rebasees.push_back(other_rebasee->Transform(env));
      }
   }

   template <Bits bits>
   BindInfo<bits>::BindInfo(const BindInfo<opposite<bits>>& other,
                            TransformEnv<opposite<bits>>& env) {
      for (const auto other_bindee : other.bindees) {
         bindees.push_back(other_bindee->Transform(env));
      }
   }

   template <Bits bits>
   RebaseNode<bits>::RebaseNode(const RebaseNode<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env):
      type(other.type), blob(nullptr)
   {
      env.resolve(other.blob, &blob);
   }

   template <Bits bits>
   BindNode<bits>::BindNode(const BindNode<opposite<bits>>& other,
                            TransformEnv<opposite<bits>>& env):
      type(other.type), addend(other.addend), dylib(nullptr), sym(other.sym), flags(other.flags),
      blob(nullptr)
   {
      env.resolve(other.dylib, &dylib);
      env.resolve(other.blob, &blob);
   }

   template <Bits bits>
   ExportInfo<bits>::ExportInfo(const Image& img, std::size_t offset, std::size_t size,
                                ParseEnv<bits>& env) {
      if (size > 0) {
         trie.decode(Pos(img, offset), ParseNode);
      }
   }

   template <Bits bits>
   ExportNode<bits>::ExportNode(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                                std::size_t size) {
      offset += leb_decode(img, offset, flags);

      if ((flags & EXPORT_SYMBOL_FLAGS_REEXPORT)) {
         throw error("export node with flags EXPORT_SYMBOL_FLAGS_REEXPORT not supported");
      } else {
         offset += leb_decode(img, offset, value);
      }
   }

   template <Bits bits>
   typename ExportInfo<bits>::NodeInfo ExportInfo<bits>::ParseNode(Pos pos) {
      if (pos.str.empty()) {
         std::size_t size;
         pos.offset += leb128_decode(pos.img, pos.offset, size);
         
         /* create new real node */
         ExportNode<bits> node = ExportNode<bits>::Parse(pos.img, pos.offset, pos.env, size);
         pos.offset += size;

         /* get edges */
         uint8_t nedges = pos.img.template at<uint8_t>(pos.offset++);
         Edges edges;
         for (uint8_t i = 0; i < nedges; ++i) {
            char *sym = &pos.img.template at<char>(pos.offset);
            pos.offset += strlen(sym) + 1;
            assert(*sym != '\0');

            std::size_t edge_offset;
            pos.offset += leb128_decode(pos.img, pos.offset, edge_offset);

            // sym + 1 since we're processing one char
            edges.emplace_back(*sym, Pos(pos.img, pos.env, edge_offset, sym + 1));
         }

         return {edges, node};
      } else {
         /* create dummy node */
         char c = pos.str.front();
         pos.str.erase(pos.str.begin());
         Edge edge(c, pos);
         return {{edge}, std::nullopt};
      }
   }
   

   template class DyldInfo<Bits::M32>;
   template class DyldInfo<Bits::M64>;

}
