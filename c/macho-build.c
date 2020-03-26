#include <string.h>


#include "macho.h"
#include "macho-util.h"
#include "macho-build.h"
#include "macho-sizeof.h"
#include "util.h"

static int macho_build_segment_32_LINKEDIT(struct segment_32 *segment, struct build_info *info);

#define MACHO_BITS 32
#include "macho-build-t.c"
//#define MACHO_BITS 64
//#include "macho-build-t.c"


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

int macho_build_archive_32(struct archive_32 *archive, struct build_info *info) {
   /* recompute size of commands */
   size_t sizeofcmds = 0;
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      sizeofcmds += macho_sizeof_load_command_32(&archive->commands[i]);
   }
   archive->header.sizeofcmds = sizeofcmds;

   /* update data offset */
   info->dataoff += sizeofcmds + sizeof(archive->header);

   /* build each command */
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      if (macho_build_load_command_32(&archive->commands[i], info) < 0) {
         return -1;
      }
   }

   return 0;
}

int macho_build_load_command_32(union load_command_32 *command, struct build_info *info) {
   switch (command->load.cmd) {
   case LC_SEGMENT:
      return macho_build_segment_32(&command->segment, info);
            
   case LC_LOAD_DYLINKER:
   case LC_UUID:
   case LC_UNIXTHREAD:
   case LC_VERSION_MIN_MACOSX:
   case LC_SOURCE_VERSION:
   case LC_LOAD_DYLIB:
      /* nothing to adjust */
      return 0;

      /* linkedit commands are adjusted by __LINKEDIT segment */
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      return 0;

   case LC_MAIN:
      /* command is adjusted in __TEXT,__text section */
      return 0;

   case LC_DYLD_INFO_ONLY:
   case LC_SYMTAB:
   case LC_DYSYMTAB:
      /* command is build in __LINKEDIT segment */
      return 0;

   default:
      fprintf(stderr, "macho_build_load_command_32: unrecognized load command type 0x%x\n",
              command->load.cmd);
      return -1;
   }
}



int macho_build_symtab_32(struct symtab_32 *symtab, struct build_info *info) {
   /* allocate data region */
   symtab->command.symoff = info->dataoff;
   info->dataoff += symtab->command.nsyms * sizeof(symtab->entries[0]);
   symtab->command.stroff = info->dataoff;
   info->dataoff += symtab->command.strsize;

   // DEBUG
   printf("symtab dataoff = 0x%x; symtab dataend = 0x%llx\n", symtab->command.symoff,
          info->dataoff);
   
   return 0;
}

int macho_build_dysymtab_32(struct dysymtab_32 *dysymtab, struct build_info *info) {
   if (dysymtab->command.ntoc) {
      fprintf(stderr,
              "macho_parse_dysymtab_32: parsing table of contents not supported\n");
      return -1;
   }
   if (dysymtab->command.nmodtab) {
      fprintf(stderr,
              "macho_parse_dysymtab_32: parsing module table not supported\n");
      return -1;
   }
   if (dysymtab->command.nextrefsyms) {
      fprintf(stderr,
              "macho_parse_dysymtab_32: parsing referenced symbol table not supported\n");
      return -1;
   }
   if (dysymtab->command.nextrel) {
      fprintf(stderr,
              "macho_parse_dysymtab_32: parsing external relocation entries not supported\n");
      return -1;
   }
   if (dysymtab->command.nlocrel) {
      fprintf(stderr,
              "macho_parse_dysymtab_32: parsing local relocation entries not supported\n");
      return -1;
   }

   /* build indirect symtab */
   dysymtab->command.indirectsymoff = info->dataoff;
   info->dataoff += dysymtab->command.nindirectsyms * sizeof(dysymtab->indirectsyms[0]);
   
   return 0;
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
