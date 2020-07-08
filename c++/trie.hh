#pragma once

#include <unordered_map>
#include <memory>
#include <list>
#include <vector>

template <typename T, typename U>
class trie {
   class node;
   using children_t = std::unordered_map<T, std::unique_ptr<node>>;
public:
   class iterator {
   public:
      U operator*() const {
         std::vector<T> values(its.size());
         std::transform(its.begin(), its.end(), values.begin(),
                        [] (auto it) { return it->first; });
         return U(values.begin(), values.end());
      }

      iterator& operator++() {
         /* invariant: non-end iterator points to valid node */
         assert(its.back()->second->valid);
         if (!its.back()->second->children.empty()) {
            /* descend until valid */
            do {
               maps.push_back(&its.back()->second->children);
               its.push_back(maps.back()->begin());
            } while (!its.back()->second->valid);
         } else {
            while (!its.empty() && ++its.back() == maps.back()->end()) {
               its.pop_back();
               maps.pop_back();
            }

            if (!its.empty()) {
               /* descend until valid */
               while (!its.back()->second->valid) {
                  maps.push_back(&its.back()->second->children);
                  its.push_back(maps.back()->begin());
               }
            }
         }
         
         return *this;
      }

      bool operator==(const iterator& other) const { return its == other.its; }
      bool operator!=(const iterator& other) const { return !(*this == other); }
         
   private:
      std::list<typename children_t::iterator> its;
      std::list<children_t *> maps;
      friend class trie<T, U>;
   };
      
   template <typename It>
   iterator insert(It begin, It end) {
      iterator current_iterator;
      node *node = &root;
      for (It it = begin; it != end; ++it) {
         auto child = node->children.insert({*it, std::make_unique<class node>(false)});

         current_iterator.its.push_back(child.first);
         current_iterator.maps.push_back(&node->children);
            
         node = child.first->second.get();
      }

      node->valid = true;
      ++size_;
      return current_iterator;
   }

   iterator insert(const U& elem) { return insert(elem.begin(), elem.end()); }

   iterator erase(iterator pos) {
      while (!pos.its.empty() && pos.its.back()->second->children.empty()) {
         pos.maps.back()->erase(pos.its.back());
         pos.maps.pop_back();
         pos.its.pop_back();
      }

      --size_;
      
   }
      
   iterator begin() {
      iterator begin_iterator;
      if (!root.children.empty()) {
         node *node = &root;
         while (!node->valid) {
            begin_iterator.its.push_back(node->children.begin());
            begin_iterator.maps.push_back(&node->children);
            node = node->children.begin()->second.get();
         }
      }
      return begin_iterator;
   }

   iterator end() { return iterator(); }

   using size_type = std::size_t;
   size_type size() const { return size_; }

   trie(): root(false) {}

private:
   struct node {
      bool valid;
      children_t children;

      node(bool valid): valid(valid) {}
   };

   node root;
   size_type size_;
};

