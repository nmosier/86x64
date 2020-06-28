#pragma once

#include <unordered_map>
#include <list>

namespace MachO {

   template <typename T, typename U>
   class Resolver {
   public:
      using FoundMap = std::unordered_map<T, const U *>;
      typedef void (*Callback)(const U *); /*!< callback for resolved values */
      using TodoNode = std::pair<const U **, Callback>;
      using TodoMap = std::unordered_map<T, std::list<TodoNode>>;
      
      void add(const T& key, const U *pointee) {
         auto todo_it = todo.find(key);
         if (todo_it != todo.end()) {
            for (const TodoNode& node : todo_it->second) {
               node.second(*node.first = pointee);
            }
            todo.erase(todo_it);
         }
         found.insert({key, pointee});
      }
      
      void resolve(const T& key, const U **pointer, Callback callback = [] (const U *value) {}) {
         auto found_it = found.find(key);
         if (found_it != found.end()) {
            callback(*pointer = found_it->second);
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

   private:
      FoundMap found;
      TodoMap todo;
   };   
   
}
