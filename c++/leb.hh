#pragma once

#include <stdexcept>

#include "leb.h"

template <typename T>
size_t leb128_decode(const void *buf, size_t buflen, T& n) {
   static_assert(std::is_integral<T>());
   size_t count;
   std::string name;
   if constexpr (std::is_unsigned<T>()) {
         count = uleb128_decode(buf, buflen, &n);
         name = "uleb";
      } else {
      count = sleb128_decode(buf, buflen, &n);
      name = "sleb";
   }

   if (count == 0) {
      throw std::overflow_error(name + "128 overflow");
   } else if (count > buflen) {
      throw std::invalid_argument(name + "128 runs past end of buffer");
   }

   return count;
}
