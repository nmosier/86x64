#pragma once

#include <unordered_map>
#include <list>

namespace MachO {
   template <Bits bits> class SectionBlob;

   template <Bits bits>
   class ParseEnv {
   public:
      using AddrMap = std::unordered_map<std::size_t, SectionBlob<bits> *>;
      using TodoMap = std::unordered_map<std::size_t, std::list<const SectionBlob<bits> **>>;
      
      AddrMap addr_map;
      TodoMap todo_map;
      
   private:
   };
   
}
