#pragma once

#include <map>
#include <list>
#include <memory>

#include "util.hh"

namespace MachO {

   template <typename T, typename U>
   class Resolver {
   public:
      struct functor {
         virtual void operator()(U *val) = 0;
         virtual ~functor() {}
      };

      struct noop: public functor {
         virtual void operator()(U *val) override {}
      };
      
      using FoundMap = std::map<T, U *>;
      // typedef void (*Callback)(U *, Resolver<T, U>&);
      using TodoNode = std::pair<const U **, std::shared_ptr<functor>>;
      using TodoMap = std::map<T, std::list<TodoNode>>;
      
      void add(const T& key, U *pointee) {
         auto todo_it = todo.find(key);
         if (todo_it != todo.end()) {
            for (const TodoNode& node : todo_it->second) {
               *node.first = pointee;
               (*node.second)(pointee);
            }
            todo.erase(todo_it);
         }
         found.insert({key, pointee});
      }

      void resolve(const T& key, const U **pointer,
                   std::shared_ptr<functor> callback = std::make_shared<noop>()) {
         auto found_it = found.find(key);
         if (found_it != found.end()) {
            *pointer = found_it->second;
            (*callback)(found_it->second);
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

      FoundMap found;
      TodoMap todo;

   };   
   
}
