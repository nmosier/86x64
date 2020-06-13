#include <stdio.h>
#include <stdlib.h>
#include "util.h"

#define MAX_STDIN 4096

int main(int argc, char *argv[]) {
   int retv = 0;

   /* encode args */
   for (int i = 1; i < argc; ++i) {
      const char *arg = argv[i];
      char *end;

      intmax_t val = strtol(arg, &end, 0);
      if (*end != '\0' || *arg == '\0') {
         fprintf(stderr, "%s: strtoul: bad integer '%s'\n", argv[0], arg);
         retv = 1;
      } else {
         uint8_t buf[32];
         size_t size = sleb128_encode(buf, 32, val);
         printf("size=%zu, bytes=", size);
         for (uint8_t *it = buf; it != buf + size; ++it) {
            printf("%02x", *it);
         }
         printf("\n");
      }
   }

   /* decode stdin */
   uint8_t buf[MAX_STDIN];
   uint8_t *buf_wptr = buf;
   uint8_t *buf_rptr = buf;
   int c;
   while ((c = getchar()) != EOF) {
      *buf_wptr++ = c;
      intmax_t sleb;
      size_t size = sleb128_decode(buf_rptr, buf_wptr - buf_rptr, &sleb);
      if (size <= buf_wptr - buf_rptr) {
         if (size == 0) {
            fprintf(stderr, "%s: sleb128_decode: overflowed\n", argv[0]);
            retv = 1;
         } else {
            /* success */
            printf("decoded %jd\n", sleb);
         }
         buf_wptr = buf_rptr = buf;
      }
   }
   
   return retv;
}
