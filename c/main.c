#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include <mach-o/loader.h>

#include "macho.h"
#include "macho-parse.h"
#include "macho-emit.h"
#include "macho-build.h"

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

   /* parse 32-bit Mach-O */
   union macho macho;
   if (macho_parse(f32, &macho) < 0) {
      retv = 5;
      goto cleanup;
   }

   printf("[before] macho.sizeofcmds=0x%x\n", macho.archive.archive_32.header.sizeofcmds);
   
   /* build 32-bit Mach-O */
#if 1
   if (macho_build(&macho) < 0) {
      retv = 7;
      goto cleanup;
   }
#endif

   printf("[after] macho.sizeofcmds=0x%x\n", macho.archive.archive_32.header.sizeofcmds);   

   /* write 32-bit Mach-O */
   if (macho_emit(f64, &macho) < 0) {
      retv = 6;
      goto cleanup;
   }

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
