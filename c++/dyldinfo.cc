#include "dyldinfo.hh"
#include "image.hh"
#include "leb.hh"
#include "macho.hh"

namespace MachO {

   template <Bits bits>
   DyldInfo<bits>::DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      dyld_info(img.at<dyld_info_command>(offset)),
      rebase(RebaseInfo<bits>::Parse(img, dyld_info.rebase_off, dyld_info.rebase_size, env))
   {
      bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld_info.bind_off),
                                  &img.at<uint8_t>(dyld_info.bind_off +
                                                   dyld_info.bind_size));
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
            vmaddr = do_rebase_times(imm, vmaddr, env);
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr = do_rebase_times(imm, vmaddr, env);
            break;

         case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr = do_rebase(vmaddr, env);
            vmaddr += uleb;
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb2);
            vmaddr = do_rebase_times(uleb, vmaddr, env, uleb2);
            break;
            
         default:
            throw error("%s: invalid rebase opcode", __FUNCTION__);
         }
         
      }
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase(std::size_t vmaddr, ParseEnv<bits>& env) {
      rebasees.push_back(nullptr);
      env.resolve(vmaddr, &rebasees.back());
      return vmaddr + sizeof(ptr_t);
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase_times(std::size_t count, std::size_t vmaddr,
                                                 ParseEnv<bits>& env, std::size_t skipping) {
      for (std::size_t i = 0; i < count; ++i) {
         do_rebase(vmaddr, env);
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
      std::vector<DylibCommand<bits> *> dylibs =
         env.archive.template subclass<DylibCommand<bits>, LC_LOAD_DYLIB>();
      DylibCommand<bits> *load_dylib = nullptr;
      const char *sym = nullptr;
      uint8_t flags = 0;
      std::size_t addend = 0;
      Segment<bits> *segment = nullptr;
               
      while (it != end) {
         const uint8_t byte = img.at<uint8_t>(it);
         const uint8_t opcode = byte & BIND_OPCODE_MASK;
         const uint8_t imm = byte & BIND_IMMEDIATE_MASK;

         ++it;

         ssize_t uleb, uleb2;

         switch (opcode) {
         case BIND_OPCODE_DONE:
            return;

         case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
            load_dylib = dylibs.at(imm);
            break;

         case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            load_dylib = dylibs.at(uleb);
            break;

         case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
            throw error("%s: BIND_OPCODE_SET_DYLIB_SPECIAL_IMM not supported", __FUNCTION__);

         case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
            flags = imm;
            sym = &img.at<char>(it);
            it += strnlen(sym, end - it);
            break;

         case BIND_OPCODE_SET_TYPE_IMM:
            type = imm;
            break;

         case BIND_OPCODE_SET_ADDEND_SLEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, addend);
            break;

         case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, vmaddr);
            vmaddr += env.archive.segment(imm)->segment_command.vmaddr;
            break;

         case BIND_OPCODE_ADD_ADDR_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr += uleb;
            break;

         case BIND_OPCODE_DO_BIND:
            do_bind(vmaddr, env, type, addend, load_dylib, sym, flags);
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
            vmaddr = do_bind(vmaddr, env, type, addend, load_dylib, sym, flags);
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            vmaddr += uleb;
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
            vmaddr = do_bind(vmaddr, env, type, addend, load_dylib, sym, flags);
            vmaddr += imm * sizeof(ptr_t);
            break;

         case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb);
            it += leb128_decode(&img.at<uint8_t>(it), end - it, uleb2);
            vmaddr = do_bind_times(uleb, vmaddr, env, type, addend, load_dylib, sym, flags, uleb2);
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
                                       ssize_t addend, DylibCommand<bits> *dylib, const char *sym,
                                       uint8_t flags) {
      bindees.push_back(BindInfo<bits>::Parse(vmaddr, env, type, addend, dylib, sym, flags));
      return vmaddr + sizeof(ptr_t);
   }

   template <Bits bits>
   std::size_t BindInfo<bits>::do_bind_times(std::size_t count, std::size_t vmaddr,
                                             ParseEnv<bits>& env, uint8_t type, ssize_t addend,
                                             DylibCommand<bits> *dylib, const char *sym,
                                             uint8_t flags, std::size_t skipping) {
      for (std::size_t i = 0; i < count; ++i) {
         vmaddr = do_bind(vmaddr, env, type, addend, dylib, sym, flags);
         vmaddr += skipping;
      }
      return vmaddr;
   }

   template <Bits bits>
   BindNode<bits>::BindNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend,
                            DylibCommand<bits> *dylib, const char *sym, uint8_t flags):
      type(type), addend(addend), dylib(dylib), sym(sym), flags(flags), blob(nullptr)
   {
      env.resolve(vmaddr, &blob);
   }
   
   
   template class DyldInfo<Bits::M32>;
   template class DyldInfo<Bits::M64>;
   
}
