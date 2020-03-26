#include <string.h>


#include "macho.h"
#include "macho-util.h"
#include "macho-build.h"
#include "macho-sizeof.h"
#include "util.h"

static int macho_build_segment_32_LINKEDIT(struct segment_32 *segment, struct build_info *info);

#define MACHO_BITS 32
#include "macho-build-t.c"
#define MACHO_BITS 64
#include "macho-build-t.c"


int macho_build(union macho *macho) {
   struct build_info info = {0};
   
   switch (macho_kind(macho)) {
   case MACHO_FAT:
      fprintf(stderr, "macho_build: building Mach-O fat binaries not supported\n");
      return -1;
      
   case MACHO_ARCHIVE:
      info.archive = &macho->archive;
      return macho_build_archive(&macho->archive, &info);
   }
}

int macho_build_archive(union archive *archive, struct build_info *info) {
   switch (macho_bits(archive)) {
   case MACHO_32:
      return macho_build_archive_32(&archive->archive_32, info);
      
   case MACHO_64:
      fprintf(stderr, "macho_build_archive: building 64-bit Mach-O archives not supported\n");
      return -1;
   }
}


int macho_build_dyld_info(struct dyld_info *dyld_info, struct build_info *info) {
   /* build rebase data */
   dyld_info->command.rebase_off = info->dataoff;
   info->dataoff += dyld_info->command.rebase_size;
   
   /* build bind data */
   dyld_info->command.bind_off = info->dataoff;
   info->dataoff += dyld_info->command.bind_size;
   
   /* build lazy bind data */
   dyld_info->command.lazy_bind_off = info->dataoff;
   info->dataoff += dyld_info->command.lazy_bind_size;
   
   /* build export data */
   dyld_info->command.export_off = info->dataoff;
   info->dataoff += dyld_info->command.export_size;
   
   return 0;
}
