#pragma once

#include <unordered_map>
#include <list>

namespace MachO {

   template <typename T, typename U>
   class Resolver {
   public:
      using FoundMap = std::unordered_map<T, const U *>;
      using TodoMap = std::unordered_map<T, std::list<const U **>>;
      
      void add(const T& key, const U *pointee) {
         auto todo_it = todo.find(key);
         if (todo_it != todo.end()) {
            for (const U **pointer : todo_it->second) { *pointer = pointee; }
            todo.erase(todo_it);
         }
         found.insert({key, pointee});
      }
      
      void resolve(const T& key, const U **pointer) {
         auto found_it = found.find(key);
         if (found_it != found.end()) {
            *pointer = found_it->second;
         } else {
            todo[key].push_back(pointer);
         }
      }

      ~Resolver() {
         if (!todo.empty()) {
            fprintf(stderr, "%s: pending unresolved pointers\n", __FUNCTION__);
            for (auto pair : todo) {
               fprintf(stderr, "key 0x%zx\n", (std::size_t) pair.first);
            }
         }
      }

   private:
      FoundMap found;
      TodoMap todo;
   };   
   
}
