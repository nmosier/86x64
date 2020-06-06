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

size_t uleb128_decode(void *buf, size_t buflen, uintmax_t *n) {
   unsigned int bits = 0;
   uintmax_t acc = 0;
   const uint8_t *it = buf;
   while (true) {
      if (it == buf + buflen) {
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

   *n = acc;
   return it - (const uint8_t *) buf;
}

size_t uleb128_encode(void *buf, size_t buflen, uintmax_t n) {
   uint8_t *it = buf;
   do {
      if (buf && it == buf + buflen) {
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

