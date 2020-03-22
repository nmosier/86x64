#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include <mach-o/loader.h>

#include "util.h"
#include "macho-parse.h"
#include "offsets.h"

int main(int argc, char *argv[]) {
   int retv = 0;
   const char *path32, *path64;
   FILE *f32 = NULL, *f64 = NULL;

   if (argc < 2 || argc > 3) {
      fprintf(stderr, "usage: %s <macho32> [<macho64>]\n", argv[0]);
      return 1;
   }

   path32 = argv[1];
   path64 = argc == 3 ? argv[2] : "a.out";

   if ((f32 = fopen(path32, "r")) == NULL) {
      fprintf(stderr, "fopen: '%s': %s\n", path32, strerror(errno));
      retv = 2;
      goto cleanup;
   }
   if ((f64 = fopen(path64, "w")) == NULL) {
      fprintf(stderr, "fopen: '%s': %s\n", path64, strerror(errno));
      retv = 2;
      goto cleanup;
   }

#if 1
   /* parse 32-bit Mach-O */
   union macho macho;
   if (macho_parse(f32, &macho) < 0) {
      retv = 5;
      goto cleanup;
   }

   ssize_t noffs;
   uint32_t **offsets;
   if ((noffs = macho_offsets(&macho, &offsets)) < 0) {
      retv = 6;
      goto cleanup;
   }
   printf("noffs=%zd\n", noffs);
   
#else
   /* turn into 64-bit? */
   union {
      struct mach_header hdr32;
      struct mach_header_64 hdr64;
   } un;
   if (fread_exact(&un.hdr32, sizeof(un.hdr32), 1, f32) < 0) {
      retv = 5;
      goto cleanup;
   }
   un.hdr64.reserved = 0;
   un.hdr64.magic = MH_MAGIC_64;
   fwrite(&un.hdr64, sizeof(un.hdr64), 1, f64);
   int c;
   while ((c = fgetc(f32)) != EOF) {
      fputc(c, f64);
   }
   
#endif

   /* success */

 cleanup:
   if (f32 && fclose(f32) < 0) {
      fprintf(stderr, "fclose: '%s': %s\n", path32, strerror(errno));
      if (retv == 0) {
         retv = 2;
      }
   }
   if (f64 && fclose(f64) < 0) {
      fprintf(stderr, "close: '%s': %s\n", path64, strerror(errno));
      if (retv == 0) {
         retv = 2;
      }
   }
   
   return retv;
}
