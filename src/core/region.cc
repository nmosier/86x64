#include <cassert>

#include "region.hh"

namespace MachO {

   void Regions::insert(std::size_t begin, std::size_t size) {
      map.insert({begin, size});
   }

   bool Regions::contains(std::size_t value) const {
      auto it = map.upper_bound(value);

      if (it == map.begin()) {
         return false;
      }

      --it;

      assert(it->first <= value);

      return value < it->first + it->second;
   }

}
