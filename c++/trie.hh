#pragma once

#include <unordered_map>
#include <list>
#include <vector>

template <typename T, typename U>
class trie {
protected:
   struct node;
   using children_t = std::unordered_map<T, node>;
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
         assert(its.back()->second.valid);
         if (!its.back()->second.children.empty()) {
            /* descend until valid */
            do {
               maps.push_back(&its.back()->second.children);
               its.push_back(maps.back()->begin());
            } while (!its.back()->second.valid);
         } else {
            while (!its.empty() && ++its.back() == maps.back()->end()) {
               its.pop_back();
               maps.pop_back();
            }

            if (!its.empty()) {
               /* descend until valid */
               while (!its.back()->second.valid) {
                  maps.push_back(&its.back()->second.children);
                  its.push_back(maps.back()->begin());
               }
               assert(its.back()->second.valid);
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
   std::pair<iterator,bool> insert(It begin, It end) {
      iterator current_iterator;
      node *node = &root;
      for (It it = begin; it != end; ++it) {
         auto child = node->children.emplace(*it, false);

         current_iterator.its.push_back(child.first);
         current_iterator.maps.push_back(&node->children);
            
         node = &child.first->second;
      }

      if (!node->valid) {
         node->valid = true;
         ++size_;
         return {current_iterator, true};
      } else {
         return {current_iterator, false};
      }
   }

   std::pair<iterator, bool> insert(const U& elem) { return insert(elem.begin(), elem.end()); }

   iterator erase(iterator pos) {
      assert(!pos.its.empty());
      assert(pos.its.back()->second.valid);
      pos.its.back()->second.valid = false;

      while (!pos.its.empty() &&
             !pos.its.back()->second.valid &&
             pos.its.back()->second.children.empty())
         {
            pos.its.back() = pos.maps.back()->erase(pos.its.back());
            if (pos.its.back() == pos.maps.back()->end()) {
               pos.its.pop_back();
               pos.maps.pop_back();
            } else {
               break;
            }
         }
         
      --size_;

      /* descend until valid */
      while (!pos.its.empty() && !pos.its.back()->second.valid) {
         pos.maps.push_back(&pos.its.back()->second.children);
         pos.its.push_back(pos.maps.back()->begin());
      }

      return pos;
   }
      
   iterator begin() {
      iterator begin_iterator;
      if (!root.children.empty()) {
         node *node = &root;
         while (!node->valid) {
            begin_iterator.its.push_back(node->children.begin());
            begin_iterator.maps.push_back(&node->children);
            node = &node->children.begin()->second;
         }
      }
      return begin_iterator;
   }

   iterator end() { return iterator(); }

   using size_type = std::size_t;
   size_type size() const { return size_; }
   bool empty() const { return size() == 0; }

   trie(): root(false) {}

protected:
   struct node {
      bool valid;
      children_t children;

      node(bool valid): valid(valid) {}
   };

   node root;
   size_type size_;
};

