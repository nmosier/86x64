#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "util.h"

int fread_exact(void *ptr, size_t size, size_t nitems, FILE *stream) {
   if (fread(ptr, size, nitems, stream) < nitems) {
      if (feof(stream)) {
         fprintf(stderr, "fread_exact: unexpected end of file\n");
      } else {
         perror("fread");
      }
      return -1;
   }
   return 0;
}

int fwrite_exact(const void *ptr, size_t size, size_t nitems, FILE *stream) {
   if (fwrite(ptr, size, nitems, stream) < nitems) {
      perror("fwrite");
      return -1;
   }
   return 0;
}

void *fmread(size_t size, size_t nitems, FILE *stream) {
   void *buf;

   if ((buf = calloc(nitems, size)) == NULL) {
      perror("calloc");
      return NULL;
   }

   if (fread_exact(buf, size, nitems, stream) < 0) {
      free(buf);
      return NULL;
   }

   return buf;
}

ssize_t merge(const void *collection[], const size_t counts[], size_t narrs, size_t itemsize,
              void **merged) {
   void *merged_local;
   
   /* sum counts */
   size_t total = 0;
   for (size_t i = 0; i < narrs; ++i) {
      total += counts[i];
   }

   /* allocate merged array */
   if ((merged_local = calloc(total, itemsize)) == NULL) {
      perror("calloc");
      return -1;
   }

   /* move into merged array */
   void *merged_it = merged_local;
   for (size_t i = 0; i < narrs; ++i) {
      size_t bytes = itemsize * counts[i];
      memcpy(merged_it, collection[i], bytes);
      merged_it = (char *) merged_it + bytes;
   }

   /* assign to output */
   *merged = merged_local;

   return total;
}

size_t fwrite_at(const void *ptr, size_t size, size_t nitems, FILE *stream, off_t offset) {
   if (fseek(stream, offset, SEEK_SET) < 0) {
      perror("fseek");
      return -1;
   }
   return fwrite_exact(ptr, size, nitems, stream);
}

size_t fread_at(void *ptr, size_t size, size_t nitems, FILE *stream, off_t offset) {
   if (fseek(stream, offset, SEEK_SET) < 0) {
      perror("fseek");
      return -1;
   }
   return fread_exact(ptr, size, nitems, stream);
}

void *fmread_at(size_t size, size_t nitems, FILE *stream, off_t offset) {
   if (fseek(stream, offset, SEEK_SET) < 0) {
      perror("fread_at: fseek");
      return NULL;
   }
   return fmread(size, nitems, stream);
}

void *memdup(const void *buf, size_t size) {
   void *newbuf;
   if ((newbuf = malloc(size)) == NULL) { return NULL; }
   memcpy(newbuf, buf, size);
   return newbuf;
}

/**************
 * LEB128 API *
 **************/

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

      acc += (byte & ULEB128_DATAMASK) << bits;

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
      sacc &= (-1 << (bits + 7)) | ((byte & SLEB128_DATAMASK) << bits) | ((1 << bits) - 1);
      
      signbit = (byte & SLEB128_SIGNBIT);

      bits += 7;
      
      if (!(byte & SLEB128_CTRLMASK)) {
         *n = signbit ? sacc : uacc;
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
