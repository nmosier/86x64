#pragma once

#include <map>
#include <list>
#include <memory>

#include "util.hh"

namespace MachO {

   template <typename T, typename U, bool lazy>
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
         if constexpr (!lazy) {
               auto todo_it = todo.find(key);
               if (todo_it != todo.end()) {
                  for (const TodoNode& node : todo_it->second) {
                     *node.first = pointee;
                     (*node.second)(pointee);
                  }
                  todo.erase(todo_it);
               }
            }
         assert(found.find(key) == found.end());
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

      void do_resolve() {
         for (auto todo_it = todo.begin(); todo_it != todo.end(); ++todo_it) {
            if (!todo_it->second.empty()) {
               auto found_it = found.find(todo_it->first);
               if (found_it != found.end()) {
                  for (const TodoNode& node : todo_it->second) {
                     *node.first = found_it->second;
                     (*node.second)(found_it->second);
                  }
               }
            }
         }
      }
      
      ~Resolver() {}

      FoundMap found;
      TodoMap todo;

   };   
   
}
