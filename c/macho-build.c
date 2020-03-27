#include <string.h>


#include "macho.h"
#include "macho-util.h"
#include "macho-build.h"
#include "macho-sizeof.h"
#include "util.h"

// static int macho_build_segment_32_LINKEDIT(struct segment_32 *segment, struct build_info *info);

#define MACHO_BITS 32
#include "macho-build-t.c"
#define MACHO_BITS 64
#include "macho-build-t.c"


int macho_build(union macho *macho) {
   switch (macho_kind(macho)) {
   case MACHO_FAT:
      fprintf(stderr, "macho_build: building Mach-O fat binaries not supported\n");
      return -1;
      
   case MACHO_ARCHIVE:
      return macho_build_archive(&macho->archive);
   }
}

int macho_build_archive(union archive *archive) {
   switch (macho_bits(archive)) {
   case MACHO_32:
      {
         struct build_info_32 info = {0};
         info.archive = &archive->archive_32;
         return macho_build_archive_32(&archive->archive_32, &info);
      }
      
   case MACHO_64:
      {
         struct build_info_64 info = {0};
         info.archive = &archive->archive_64;
         return macho_build_archive_64(&archive->archive_64, &info);
      }
   }
}


