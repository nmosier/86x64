#pragma once

#include <unordered_map>
#include <list>

#include "macho-fwd.hh"
#include "loc.hh"

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
               fprintf(stderr, "key 0x%zx\n", pair.first);
            }
            abort();
         }
      }

   private:
      FoundMap found;
      TodoMap todo;
   };

   template <Bits bits>
   class DylibResolver {
   public:

      void add(const DylibCommand<bits> *pointee) { resolver.add(++id, pointee); }
      template <typename... Args>
      void resolve(Args&&... args) { return resolver.resolve(args...); }
      
      DylibResolver(): id(0) {}
      
   private:
      Resolver<DylibCommand<bits>> resolver;
      unsigned id;
   };
   
   
   template <Bits bits>
   class ParseEnv {
   public:
      Archive<bits>& archive;

      Resolver<SectionBlob<bits>> vmaddr_resolver;
      Resolver<SectionBlob<bits>> offset_resolver;
      DylibResolver<bits> dylib_resolver;

      ParseEnv(Archive<bits>& archive): archive(archive) {}
      
   private:
      
   };
   
}
