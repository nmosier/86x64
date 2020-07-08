#pragma once

#include <unordered_map>
#include <memory>
#include <list>

namespace MachO {

   template <typename T, typename U>
   class trie {
   public:
      class iterator {
      public:
         U operator*() const {
            std::vector<T> values(its.size());
            std::transform(its.begin(), its.end(), values.begin(),
                           [] (const auto& pair) { return pair.first; });
            return U(values.begin(), values.end());
         }

         iterator& operator++() {
            ++its.back();
            while (its.back() == ends.back()) {
               its.pop_back();
               ends.pop_back();
               ++its.back();
            }

            while (!its.back()->second->children.empty()) {
               its.push_back(its.back()->second->children.begin());
               ends.push_back(its.back()->second->children.end());
            }
         }

         bool operator==(const iterator& other) const {
            if (its.size() != other.its.size()) {
               return false;
            }

            children_t::iterator it, other_it;
            for (it = its.begin(), other_it = other.begin();
                 it == other_it;
                 ++it, ++other_it)
               {}

            return it == other_it;
         }
         
      private:
         using path_t = std::list<children_t::iterator>;
         path_t its;
         path_t ends;
      };
      
      template <typename It>
      iterator insert(It begin, It end) {
         iterator current_iterator;
         node *node = &root;
         for (It it = begin; it != end; ++it) {
            auto child = node->children.insert({*it, std::make_unique()});

            current_iterator.its.push_back(child->first);
            current_iterator.ends.push_back(node->children.end());
            
            node = child->first->second;
         }
         
         return current_iterator;
      }
      
      iterator begin() {
         iterator begin_iterator;

         for (children_t::iterator it = root.children.begin(), end = root.children.end();
              it != end;
              it = it->second->children.begin(), end = it->second->children.end()) {
            begin_iterator.its.push_back(it);
            begin_iterator.ends.push_back(end);
         }

         return begin_iterator;
      }

      iterator end() {
         return iterator();
      }
      
   private:
      class node {
      public:
      private:
         using children_t = std::unordered_map<T, std::unique_ptr<node>>;
         children_t children;
      };

      node root;
   };

}
