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
         if (its.back()->second->children.empty()) {
            /* backtrack */
            do {
               its.pop_back();
               ends.pop_back();
               if (its.empty()) {
                  return *this;
               } else {
                  ++its.back();
               }
            } while (its.back() == ends.back());

            /* go down tree looking for next valid node */
            while (!its.back()->second->valid) {
               auto cur = its.back();
               its.push_back(cur->second->children.begin());
               ends.push_back(cur->second->children.end());
            }
         } else {
            do {
               auto cur = its.back();
               its.push_back(cur->second->children.begin());
               ends.push_back(cur->second->children.end());
            } while (!its.back()->second->valid);
         }
            
         return *this;
      }

      bool operator==(const iterator& other) const {
         if (its.size() != other.its.size()) {
            return false;
         }

         auto it = its.begin(), other_it = other.its.begin();
         for (; it != its.end() && *it == *other_it; ++it, ++other_it) {}
            
         return *it == *other_it;
      }

      bool operator!=(const iterator& other) const { return !(*this == other); }
         
   private:
      using path_t = std::list<typename children_t::iterator>;
      path_t its;
      path_t ends;
      friend class trie<T, U>;
   };
      
   template <typename It>
   iterator insert(It begin, It end) {
      iterator current_iterator;
      node *node = &root;
      for (It it = begin; it != end; ++it) {
         auto child = node->children.insert({*it, std::make_unique<class node>(false)});

         current_iterator.its.push_back(child.first);
         current_iterator.ends.push_back(node->children.end());
            
         node = child.first->second.get();
      }

      node->valid = true;
      ++size_;
      return current_iterator;
   }

   iterator insert(const U& elem) { return insert(elem.begin(), elem.end()); }

   iterator erase(iterator pos) {
      auto next_pos = ++pos;
      auto its = pos.its;
      auto ends = pos.ends;
         
      while (!its.empty() && its.back()->second->children.empty()) {
         auto to_delete = its.back();
         its.pop_back();
         ends.pop_back();
         if (its.empty()) {
            root.children.erase(to_delete);
         } else {
            its.back()->second->children.erase(to_delete);
         }
      }
      --size_;
      return next_pos;
   }
      
   iterator begin() {
      iterator begin_iterator;
      if (!root.children.empty()) {
         node *node = &root;
         while (!node->valid) {
            begin_iterator.its.push_back(node->children.begin());
            begin_iterator.ends.push_back(node->children.end());
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

