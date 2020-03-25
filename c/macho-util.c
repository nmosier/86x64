#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

#include "macho.h"
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

struct linkedit_data *macho_linkedit(union load_command_32 *command) {
   return macho_is_linkedit(command) ? &command->linkedit : NULL;
}

struct segment_32 *macho_find_segment_32(const char *segname, struct archive_32 *archive) {
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      union load_command_32 *command = &archive->commands[i];
      if (command->load.cmd == LC_SEGMENT) {
         struct segment_32 *segment = &command->segment;
         if (strncmp(segment->command.segname, segname, sizeof(segment->command.segname)) == 0) {
            return segment;
         }
      }
   }

   return NULL;
}

struct section_wrapper_32 *macho_find_section_32(const char *sectname, struct segment_32 *segment)
{
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      struct section_wrapper_32 *sectwr = &segment->sections[i];
      if (strncmp(sectwr->section.sectname, sectname, sizeof(sectwr->section.sectname)) == 0) {
         return sectwr;
      }
   }

   return NULL;
}

union load_command_32 *macho_find_load_command_32(uint32_t cmd, struct archive_32 *archive) {
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      union load_command_32 *command = &archive->commands[i];
      if (command->load.cmd == cmd) {
         return command;
      } 
   }

   return NULL;
}

int macho_off_cmp(const macho_off_t *a, const macho_off_t *b) {
   return a - b;
}


