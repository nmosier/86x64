#pragma once

#include <unordered_map>
#include <list>

#include "macho-fwd.hh"
#include "loc.hh"

namespace MachO {
   
   template <Bits bits> class SectionBlob;

#if 0
   template <typename T>
   class ResolverNode {
   public:
      ResolverNode(const T **pointer, bool required): pointer(pointer), required(required) {}

      void bind(const T *pointee) { *pointer = pointee; }
      bool ok(std::size_t offset) {
         
      }

   private:
      const T **pointer;
      bool required;
   };
#endif

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
         }
      }

   private:
      FoundMap found;
      TodoMap todo;
   };

   template <typename T>
   class CountResolver {
   public:

      void add(const T *pointee) { resolver.add(++id, pointee); }
      template <typename... Args>
      void resolve(Args&&... args) { return resolver.resolve(args...); }
      
      CountResolver(): id(0) {}
      
   private:
      Resolver<T> resolver;
      unsigned id;
   };

   template <Bits bits>
   class ParseEnv {
   public:
      Archive<bits>& archive;

      Resolver<SectionBlob<bits>> vmaddr_resolver;
      Resolver<SectionBlob<bits>> offset_resolver;
      CountResolver<DylibCommand<bits>> dylib_resolver;
      CountResolver<Segment<bits>> segment_resolver;
      Segment<bits> *current_segment;

      ParseEnv(Archive<bits>& archive): archive(archive), current_segment(nullptr) {}
      
   private:
      
   };
   
}
