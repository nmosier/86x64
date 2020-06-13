#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <xed-interface.h>

#include "macho.h"
#include "macho-patch.h"
#include "macho-util.h"
#include "util.h"

#define MACHO_BITS 32
#include "macho-patch-t.c"
#define MACHO_BITS 64
#include "macho-patch-t.c"

int macho_patch(union macho *macho) {
   switch (macho_kind(macho)) {
   case MACHO_FAT:
      fprintf(stderr, "macho_patch: patching fat binary not supported\n");
      return -1;

   case MACHO_ARCHIVE:
      return macho_patch_archive(&macho->archive);
   }
}

int macho_patch_archive(union archive *archive) {
   switch (macho_bits(archive)) {
   case MACHO_32:
      return macho_patch_archive_32(&archive->archive_32);
      
   case MACHO_64:
      return macho_patch_archive_64(&archive->archive_64);
   }
}





