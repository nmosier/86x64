#include "parse.hh"

namespace MachO {

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


   template class ParseEnv<Bits::M32>;
   template class ParseEnv<Bits::M64>;
   
}
