#include <stdlib.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

#include "macho-parse.h"
#include "macho-util.h"

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

bool macho_is_linkedit(const union load_command_32 *command) {
   switch (command->load.cmd) {
   case LC_CODE_SIGNATURE:
   case LC_SEGMENT_SPLIT_INFO:
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
   case LC_DYLIB_CODE_SIGN_DRS:
   case LC_LINKER_OPTIMIZATION_HINT:
      return true;
                  
   default:
      return false;
   }

}
