#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

#include "macho.h"
#include "macho-util.h"

#define MACHO_BITS 32
#include "macho-util-t.c"
#define MACHO_BITS 64
#include "macho-util-t.c"

enum macho_kind macho_kind(const union macho *macho) {
   switch (macho->magic) {
   case MH_MAGIC:
   case MH_CIGAM:
   case MH_MAGIC_64:
   case MH_CIGAM_64:
      return MACHO_ARCHIVE;

   case FAT_MAGIC:
   case FAT_CIGAM:
      return MACHO_FAT;

   default:
      fprintf(stderr, "macho_kind: malformed Mach-O structure\n");
      abort();
   }
}

enum macho_bits macho_bits(const union archive *archive) {
   switch (archive->magic) {
   case MH_MAGIC:
   case MH_CIGAM:
      return MACHO_32;
      
   case MH_MAGIC_64:
   case MH_CIGAM_64:
      return MACHO_64;

   default:
      fprintf(stderr, "macho_bits: malformed Mach-O archive structure\n");
      abort();
   }
}





int macho_off_cmp(const macho_off_t *a, const macho_off_t *b) {
   return a - b;
}

enum macho_endian macho_endian(uint32_t magic) {
   switch (magic) {
   case FAT_MAGIC:
   case MH_MAGIC_64:
   case MH_MAGIC:
      return MACHO_ENDIAN_SAME;

   case FAT_CIGAM:
   case MH_CIGAM_64:
   case MH_CIGAM:
      return MACHO_ENDIAN_DIFF;

   default:
      fprintf(stderr, "macho_endian: invalid endianness\n");
      abort();
   }
}


int macho_remove_command(union macho *macho) {
   switch (macho->magic) {
   case MH_CIGAM:
      
      
   case MH_MAGIC:
   default:
      fprintf(stderr, "macho_remove_command: magic number %d not supported\n", macho->magic);
      return -1;
   }
}
