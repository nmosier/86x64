#ifndef __UNIQ_VECTOR_HPP
#define __UNIQ_VECTOR_HPP

namespace zc {

   /* unique vector */
   template <typename Val, typename Key, class GetKey>
   class uniq_vector {
      typedef std::vector<Val> Vec;
      typedef std::unordered_map<Key, Val> Map;
   public:
      typedef typename Vec::iterator iterator;
      typedef typename Vec::const_iterator const_iterator;

      iterator begin() { return vec_.begin(); }
      const_iterator begin() const { return vec_.begin(); }
      const_iterator cbegin() const { return vec_.cbegin(); }
      iterator end() { return vec_.end(); }
      const_iterator end() const { return vec_.end(); }
      const_iterator cend() const { return vec_.cend(); }

      iterator find(const Key& key) {
         auto map_it = map_.find(key);
         if (map_it == map_.end()) {
            return end();
         } else {
            return std::find(begin(), end(), map_it->second);
         }
      }

      template <class InputIt, class Func>
      uniq_vector(InputIt begin, InputIt end, Func collide) {
         for (; begin != end; ++begin) {
            Key key = GetKey()(*begin);
            auto it = map_.find(key);
            if (it != map_.end()) {
               collide(it->second, *begin);
            } else {
               map_[key] = *begin;
               vec_.push_back(*begin);
            }
         }
      }

   private:
      Vec vec_;
      Map map_;
   };   


}

#endif
