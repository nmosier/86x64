#pragma once

#include <stdexcept>

#include "leb.h"
#include "image.hh"

namespace MachO {

#if 0
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
#else
template <typename T>
size_t leb128_decode(const Image& img, std::size_t offset, T& n) {
   static_assert(std::is_integral<T>());
   size_t count;
   std::string name;

   if (offset > img.size()) { throw std::out_of_range(__FUNCTION__); }
   const void *buf = &img.at<char>(offset);
   const std::size_t buflen = img.size() - offset;
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
#endif

template <typename T>
size_t leb128_size(T n) {
   static_assert(std::is_integral<T>());
   if constexpr (std::is_unsigned<T>()) {
         return uleb128_encode(nullptr, std::numeric_limits<size_t>::max(), n);
      } else {
      return sleb128_encode(nullptr, std::numeric_limits<size_t>::max(), n);
   }
}

#if 0
template <typename T>
size_t leb128_encode(void *buf, size_t buflen, T n) {
   static_assert(std::is_integral<T>());
   
   std::string name;
   size_t count;
   
   if constexpr (std::is_unsigned<T>()) {
         count = uleb128_encode(buf, buflen, n);
         name = "uleb128_encode";
      } else {
      count = sleb128_encode(buf, buflen, n);
      name = "sleb128_encode";
   }

   if (count == 0) {
      throw std::invalid_argument(name + ": needs more buffer space");
   }

   return count;
}
#else
template <typename T>
size_t leb128_encode(Image& img, std::size_t offset, T n) {
   static_assert(std::is_integral<T>());
   
   std::string name;
   const size_t buflen = leb128_size(n);
   char *buf = new char[buflen];
   
   if constexpr (std::is_unsigned<T>()) {
         assert(uleb128_encode(buf, buflen, n));
      } else {
      assert(sleb128_encode(buf, buflen, n));
   }

   img.copy(offset, buf, buflen);
   delete[] buf;
                  

   return buflen;
}
#endif

}
