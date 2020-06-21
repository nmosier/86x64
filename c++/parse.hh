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
      using Dylibs = std::unordered_map<std::size_t, const DylibCommand<bits> *>;
      using TodoDylibs = std::unordered_map<std::size_t, std::list<const DylibCommand<bits> **>>;

      Archive<bits>& archive;

      void add(std::size_t vmaddr, const SectionBlob<bits> *pointee);
      void resolve(std::size_t vmaddr, const SectionBlob<bits> **pointer);

      void add(const DylibCommand<bits> *dylib);
      void resolve(std::size_t index, const DylibCommand<bits> **dylib);

      ParseEnv(Archive<bits>& archive): archive(archive), dylib_id(0) {}
      
   private:
      AddrMap addr_map;
      TodoMap todo_map;
      std::size_t dylib_id;
      Dylibs dylibs;
      TodoDylibs todo_dylibs;
   };
   
}
