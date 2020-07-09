#pragma once

#include <unordered_map>
#include <list>
#include <vector>

template <typename T, typename U, typename V>
class trie_map {
   struct node;
   using children_t = std::unordered_map<T, node>;
public:
   class iterator {
   public:
      const std::pair<U, V>& operator*() const { return *current; }
      std::pair<U, V>& operator*() { return *current; }
      const std::pair<U, V> *operator->() const { return &*current; }
      std::pair<U, V> *operator->() { return &*current; }
      
      iterator& operator++() {
         /* invariant: non-end iterator points to valid node */
         assert(its.back()->second.value);
         if (!its.back()->second.children.empty()) {
            /* descend until valid */
            do {
               maps.push_back(&its.back()->second.children);
               its.push_back(maps.back()->begin());
            } while (!its.back()->second.value);
         } else {
            while (!its.empty() && ++its.back() == maps.back()->end()) {
               its.pop_back();
               maps.pop_back();
            }

            if (!its.empty()) {
               /* descend until valid */
               while (!its.back()->second.value) {
                  maps.push_back(&its.back()->second.children);
                  its.push_back(maps.back()->begin());
               }
               assert(its.back()->second.value);
            }
         }

         set_current();
         return *this;
      }
      
      bool operator==(const iterator& other) const { return its == other.its; }
      bool operator!=(const iterator& other) const { return !(*this == other); }
         
   private:
      std::list<typename children_t::iterator> its;
      std::list<children_t *> maps;
      std::optional<std::pair<U, V>> current;
      friend class trie_map<T, U, V>;

      void set_current() {
         if (its.empty()) {
            current = std::nullopt;
         } else {
            std::vector<T> values(its.size());
            std::transform(its.begin(), its.end(), values.begin(),
                           [] (auto it) { return it->first; });
            current = std::make_pair(U(values.begin(), values.end()), *its.back()->second.value);
         }
      }
   };
      
   template <typename It>
   std::pair<iterator, bool> insert(It begin, It end, const V& value) {
      iterator current_iterator;
      node *node = &root;
      for (It it = begin; it != end; ++it) {
         auto child = node->children.emplace(*it, std::nullopt);

         current_iterator.its.push_back(child.first);
         current_iterator.maps.push_back(&node->children);
            
         node = &child.first->second;
      }

      bool inserted;
      if ((inserted = !node->value)) {
         ++size_;
         node->value = value;
      }

      current_iterator.set_current();
      return {current_iterator, inserted};
   }

   std::pair<iterator, bool> insert(const U& elem, const V& value) {
      return insert(elem.begin(), elem.end(), value);
   }
   
   iterator erase(iterator pos) {
      assert(!pos.its.empty());
      assert(pos.its.back()->second.value);
      pos.its.back()->second.value = std::nullopt;

      while (!pos.its.empty() &&
             !pos.its.back()->second.value &&
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
      while (!pos.its.empty() && !pos.its.back()->second.value) {
         pos.maps.push_back(&pos.its.back()->second.children);
         pos.its.push_back(pos.maps.back()->begin());
      }

      pos.set_current();
      return pos;
   }
      
   iterator begin() {
      iterator begin_iterator;
      if (!root.children.empty()) {
         node *node = &root;
         while (!node->value) {
            begin_iterator.its.push_back(node->children.begin());
            begin_iterator.maps.push_back(&node->children);
            node = &node->children.begin()->second;
         }
      }

      begin_iterator.set_current();
      return begin_iterator;
   }

   iterator end() { return iterator(); }

   using size_type = std::size_t;
   size_type size() const { return size_; }
   bool empty() const { return size() == 0; }

   /**
    * @tparam Pos decoder position type
    * @tparam NodeGen functor with signature `std::pair<Edges,std::optional<V>> (Pos)',
    *   where Edges is iterable with iterator dereference yielding a std::pair<T,Pos>
    * @tparam EdgeGen functor with signature `Pos (Pos)'
    */
   template <typename Pos, typename NodeGen>
   void decode(Pos pos, NodeGen node_gen) { root.decode(pos, node_gen); }
   
private:
   struct node {
      std::optional<V> value;
      children_t children;

      node(const std::optional<V>& value = std::nullopt): value(value) {}

      template <typename Pos, typename NodeGen>
      void decode(Pos pos, NodeGen node_gen) {
         auto node_result = node_gen(pos);
         value = node_result.second;

         for (auto edge : node_result.first) {
            auto result = children.emplace(edge.first, node(std::nullopt));
            result.first->second.decode(edge.second, node_gen);
         }
      }
   };

   node root;
   size_type size_;
};

#if 0
template <typename T, typename U>
class trie_set {
   using TrieMap = trie_map<T, U, bool>
public:
   class iterator {
   public:

   private:
      typename trie_map<T, U, bool>::iterator
   };
   
private:
   trie_map<T, U, bool> map;
};
#endif

/* TODO: Need way of decoding and encoding trie.
 * decode() -- needs function to decode value (node gen) and to decode edge (edge gen) 
 *
 */
