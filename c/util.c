#include <stdlib.h>
#include <stdio.h>

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
