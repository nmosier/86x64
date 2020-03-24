#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
