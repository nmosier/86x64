#pragma once

#include <map>
#include <list>

#include "util.hh"

namespace MachO {

   template <typename T, typename U>
   class Resolver {
   public:
      using FoundMap = std::map<T, U *>;
      typedef void (*Callback)(U *, Resolver<T, U>&);
      using TodoNode = std::pair<const U **, Callback>;
      using TodoMap = std::map<T, std::list<TodoNode>>;
      
      void add(const T& key, U *pointee) {
         auto todo_it = todo.find(key);
         if (todo_it != todo.end()) {
            for (const TodoNode& node : todo_it->second) {
               *node.first = pointee;
               node.second(pointee, *this);
            }
            todo.erase(todo_it);
         }
         found.insert({key, pointee});
      }
      
      void resolve(const T& key, const U **pointer, Callback callback = CallbackNop) {
         auto found_it = found.find(key);
         if (found_it != found.end()) {
            *pointer = found_it->second;
            callback(found_it->second, *this);
         } else {
            todo[key].emplace_back(pointer, callback);
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

      static void CallbackNop(U *pointee, Resolver<T, U>& resolver) {}

      FoundMap found;
      TodoMap todo;

   };   
   
}
