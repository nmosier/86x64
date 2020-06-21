#pragma once

#include <unordered_map>
#include <list>

#include "macho-fwd.hh"

namespace MachO {
   
   template <Bits bits> class SectionBlob;

   template <typename T>
   class Resolver {
   public:
      using FoundMap = std::unordered_map<std::size_t, const T *>;
      using TodoMap = std::unordered_map<std::size_t, std::list<const T **>>;

      void add(std::size_t key, const T *pointee) {
         auto todo_it = todo.find(key);
         if (todo_it != todo.end()) {
            for (const T **pointer : todo_it->second) { *pointer = pointee; }
            todo.erase(todo_it);
         }
         found.insert({key, pointee});
      }

      void resolve(std::size_t key, const T **pointer) {
         auto found_it = found.find(key);
         if (found_it != found.end()) {
            *pointer = found_it->second;
         } else {
            todo[key].push_back(pointer);
         }
      }

      ~Resolver() {
         if (!todo.empty()) {
            fprintf(stderr, "%s: pending unresolved pointers after parsing\n", __FUNCTION__);
            for (auto pair : todo) {
               fprintf(stderr, "0x%zx\n", pair.first);
            }
            abort();
         }
      }

   private:
      FoundMap found;
      TodoMap todo;
   };
   
   
   template <Bits bits>
   class ParseEnv {
   public:
      Archive<bits>& archive;

      using Dylibs = std::unordered_map<std::size_t, const DylibCommand<bits> *>;
      using TodoDylibs = std::unordered_map<std::size_t, std::list<const DylibCommand<bits> **>>;

#if 0
      using AddrMap = std::unordered_map<std::size_t, const SectionBlob<bits> *>;
      using TodoMap = std::unordered_map<std::size_t, std::list<const SectionBlob<bits> **>>;


      void add(std::size_t vmaddr, const SectionBlob<bits> *pointee);
      void resolve(std::size_t vmaddr, const SectionBlob<bits> **pointer);
#else
      Resolver<SectionBlob<bits>> vmaddr_resolver;
      Resolver<SectionBlob<bits>> offset_resolver;
#endif

      void add(const DylibCommand<bits> *dylib);
      void resolve(std::size_t index, const DylibCommand<bits> **dylib);

      ParseEnv(Archive<bits>& archive): archive(archive), dylib_id(0) {}
      
   private:
      std::size_t dylib_id;
      Dylibs dylibs;
      TodoDylibs todo_dylibs;
   };
   
}
