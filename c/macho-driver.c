#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <mach-o/loader.h>
#include <xed-interface.h>

#include "macho.h"
#include "macho-parse.h"
#include "macho-emit.h"
#include "macho-build.h"
#include "macho-patch.h"
#include "macho-transform.h"
#include "macho-driver.h"

int macho_main(int argc, char *argv[], macho_op op) {
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

   /* initialization */
   xed_tables_init();

   /* parse 32-bit Mach-O */
   union macho macho;
   if (macho_parse(f32, &macho) < 0) {
      retv = 5;
      goto cleanup;
   }

   /* build 32-bit Mach-O */
   if (macho_build(&macho) < 0) {
      retv = 7;
      goto cleanup;
   }

   /* patch 32-bit Mach-O */
   if (macho_patch(&macho) < 0) {
      retv = 8;
      goto cleanup;
   }

   /* apply operation to Mach-O */
   union macho newmacho;
   if (op(&macho, &newmacho) < 0) {
      retv = 9;
      goto cleanup;
   }

   /* write Mach-O */
   if (macho_emit(f64, &newmacho) < 0) {
      retv = 6;
      goto cleanup;
   }

   /* make executable */
   int fd64 = fileno(f64);
   if (fchmod(fd64, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
      perror("fchmod");
      retv = 10;
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