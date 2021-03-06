#include "parse.hh"
#include "section_blob.hh"

namespace MachO {

#if 0
   template <Bits bits>
   void ParseEnv<bits>::add(std::size_t vmaddr, const SectionBlob<bits> *pointee) {
      /* check for any unresolved pointers in todo map */
      auto todo_it = todo_map.find(vmaddr);
      if (todo_it != todo_map.end()) {
         for (const SectionBlob<bits> **pointer : todo_it->second) {
            *pointer = pointee;
         }
         todo_map.erase(todo_it);
      }

      /* add to addr map */
      addr_map.insert({vmaddr, pointee});
   }

   template <Bits bits>
   void ParseEnv<bits>::resolve(std::size_t vmaddr, const SectionBlob<bits> **pointer) {
      /* check if in addr map */
      auto addr_it = addr_map.find(vmaddr);
      if (addr_it != addr_map.end()) {
         *pointer = addr_it->second;
      } else {
         todo_map[vmaddr].push_back(pointer);
      }
   }

   template <Bits bits>
   void ParseEnv<bits>::add(const DylibCommand<bits> *dylib) {
      dylibs.insert({++dylib_id, dylib});
      auto todo_it = todo_dylibs.find(dylib_id);
      if (todo_it != todo_dylibs.end()) {
         for (const DylibCommand<bits> **ptr : todo_it->second) {
            *ptr = dylib;
         }
         todo_dylibs.erase(todo_it);
      }
   }

   template <Bits bits>
   void ParseEnv<bits>::resolve(std::size_t index, const DylibCommand<bits> **dylib) {
      auto it = dylibs.find(index);
      if (it == dylibs.end()) {
         todo_dylibs[index].push_back(dylib);
      } else {
         *dylib = it->second;
      }
   }
#endif

   template <Bits bits>
   Placeholder<bits> *ParseEnv<bits>::add_placeholder(std::size_t vmaddr) {
      if (vmaddr == 0) {
         return nullptr;
      }
      
      auto it = placeholders.find(vmaddr);
      if (it == placeholders.end()) {
         Placeholder<bits> *placeholder = Placeholder<bits>::Parse(Location(0, vmaddr), *this);
         placeholders.insert({vmaddr, placeholder});
         return placeholder;
      } else {
         return it->second;
      }
   }

   template <Bits bits>
   void ParseEnv<bits>::do_resolve() {
      offset_resolver.do_resolve();
      vmaddr_resolver.do_resolve();
   }
   
   template class ParseEnv<Bits::M32>;
   template class ParseEnv<Bits::M64>;
   
}
