#include <stdbool.h>

#include "leb.h"

size_t uleb128_decode(const void *buf, size_t buflen, uintmax_t *n) {
   unsigned int bits = 0;
   uintmax_t acc = 0;
   const uint8_t *it = buf;
   const uint8_t *end = it + buflen;
   while (true) {
      if (it == end) {
         /* not enough bytes */
         return buflen + 1;
      }

      uint8_t byte = *it++;

      acc += ((uintmax_t) (byte & ULEB128_DATAMASK)) << bits;

      if (!(byte & ULEB128_CTRLMASK)) {
         break;
      }
      
      bits += 7;
      if (bits > ULEB128_MAXBITS) {
         /* overflow */
         return 0;
      }
   }

   if (n) {
      *n = acc;
   }
   return it - (const uint8_t *) buf;
}

size_t uleb128_encode(void *buf, size_t buflen, uintmax_t n) {
   uint8_t *it = buf;
   const uint8_t *end = it + buflen;
   do {
      if (buf && it == end) {
         /* buffer too short */
         return buflen + 1; // TODO -- return actual length
      }
      if (buf) {
         *it = n & ULEB128_DATAMASK;
      }
      n >>= 7;
      if (n) {
         if (buf) {
            *it |= ULEB128_CTRLMASK;
         }
      }
      ++it;
   } while (n);
   
   return it - (uint8_t *) buf;
}

size_t sleb128_decode(const void *buf, size_t buflen, intmax_t *n) {
   unsigned int bits = 0;
   intmax_t sacc = -1;
   uintmax_t uacc = 0;
   bool signbit = 0;

   const uint8_t *begin = buf;
   const uint8_t *end = begin + buflen;
   const uint8_t *it;

   for (it = begin; ; ) {
      if (it == end) {
         /* not enough bytes */
         return buflen + 1;
      }

      uint8_t byte = *(it++);

      uacc |= (byte & SLEB128_DATAMASK) << bits;
      sacc &= (-1 << (bits + 7)) | (((intmax_t) (byte & SLEB128_DATAMASK)) << bits) | ((1 << bits) - 1);
      
      signbit = (byte & SLEB128_SIGNBIT);

      bits += 7;
      
      if (!(byte & SLEB128_CTRLMASK)) {
         if (n) {
            *n = signbit ? sacc : uacc;
         }
         return it - begin;
      }

      if (bits >= SLEB128_MAXBITS) {
         /* overflow */
         return 0;
      }
   }
   
}

size_t sleb128_encode(void *buf, size_t buflen, intmax_t n) {
   uint8_t *begin = buf;
   uint8_t *end = begin + buflen;

   for (uint8_t *it = begin; ; ) {
      if (it == end) {
         /* not enough bytes */
         return buflen + 1;
      }

      const uint8_t data = n & SLEB128_DATAMASK;
      *it = data;

      n >>= 7;

      /* check if sign agrees */
      if ((!!(data & SLEB128_SIGNBIT) == (n < 0)) && (n == (n < 0 ? -1 : 0))) {
         return it - begin + 1;
      } else {
         *it = n | SLEB128_CTRLMASK;
      }

      ++it;
   }
}
