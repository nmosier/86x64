#pragma once

#include <unordered_map>
#include <list>

#include "macho-fwd.hh"

namespace MachO {
   
   template <Bits bits> class SectionBlob;

   template <Bits bits>
   class ParseEnv {
   public:
      using AddrMap = std::unordered_map<std::size_t, const SectionBlob<bits> *>;
      using TodoMap = std::unordered_map<std::size_t, std::list<const SectionBlob<bits> **>>;

      void add(std::size_t vmaddr, const SectionBlob<bits> *pointee);
      void resolve(std::size_t vmaddr, const SectionBlob<bits> **pointer);
      
   private:
      AddrMap addr_map;
      TodoMap todo_map;
   };
   
}
