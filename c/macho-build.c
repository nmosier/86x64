#include <string.h>

#include "macho-util.h"
#include "macho-build.h"
#include "macho-sizeof.h"
#include "util.h"

static int macho_build_segment_32_refs(struct archive_32 *archive);

int macho_build(union macho *macho) {
   switch (macho_kind(macho)) {
   case MACHO_FAT:
      fprintf(stderr, "macho_build: building Mach-O fat binaries not supported\n");
      return -1;
      
   case MACHO_ARCHIVE:
      return macho_build_archive(&macho->archive, 0);
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
   /* pre-processing */
   if (macho_build_segment_32_refs(archive) < 0) { return -1; }
   
   /* recompute size of commands */
   size_t sizeofcmds = 0;
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      sizeofcmds += macho_sizeof_load_command_32(&archive->commands[i]);
   }
   archive->header.sizeofcmds = sizeofcmds;

   /* build each command */
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      // TODO
   }

   return -1; // TODO
}

int macho_build_load_command_32(union load_command_32 *command, struct build_info *info) {
   switch (command->load.cmd) {
   case LC_SEGMENT:
      return macho_build_segment_32(&command->segment, info);
      
   case LC_SYMTAB:
      return macho_build_symtab_32(&command->symtab, info);

   case LC_DYSYMTAB:
      // TODO
      return -1;

   case LC_LOAD_DYLINKER:
      // TODO
      return -1;

   case LC_UUID:
      // TODO
      return -1;

   case LC_UNIXTHREAD:
      // TODO
      return -1;

   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      // TODO
      return -1;

   default:
      fprintf(stderr, "macho_build_load_command_32: unrecognized load command type 0x%x\n",
              command->load.cmd);
      return -1;
   }
}

/* NOTES:
 *  - Each segment needs to start on a page bounary, i.e. its file offset must be a multiple of
 *    0x1000.
 * QUESTIONS:
 *  - Do the first segments need to be mapped for an entire page if they only take up a small 
 *    portion of it?
 */
int macho_build_segment_32(struct segment_32 *segment, struct build_info *info) {
   /* get max alignment */
   uint32_t section_maxalign = 0;
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      section_maxalign = MAX(segment->sections[i].section.align, section_maxalign);
   }
   
   /* recompute size required for all sections */
   /* NOTE: This is conservative in alignment requirements. */
   uint32_t segment_minsize = 0;
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      segment_minsize += ALIGN_UP(segment->sections[i].section.size, 1 << section_maxalign);
   }
   uint32_t segment_minsize_aligned = ALIGN_UP(segment_minsize, PAGESIZE);

   /* allocate data region for segment */
   uint32_t segment_start = ALIGN_DOWN(info->dataoff, PAGESIZE);

   /* update segment command information */
   segment->command.fileoff = segment_start;
   segment->command.filesize = segment_minsize_aligned;
   segment->command.vmaddr = info->vmaddr;
   uint32_t vmsize = MAX(PAGESIZE, segment_minsize_aligned);
   segment->command.vmsize = vmsize;
   info->vmaddr += vmsize;

   /* update section information */
   macho_off_t dataoff = info->dataoff;
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      struct section *section = &segment->sections[i].section;
      dataoff = ALIGN_UP(dataoff, section->align);
      section->offset = dataoff;
      dataoff += section->size;
   }
   
   /* update build info */
   info->dataoff = MAX(segment_start + segment_minsize_aligned, info->dataoff);
   
   return 0;
}

int macho_build_symtab_32(struct symtab_32 *symtab, struct build_info *info) {
   /* allocate data region for symbol table */
   // uint32_t datasize = symtab->command.symoff
   fprintf(stderr, "macho_build_symtab_32: stub\n");
   return -1; 
}

/*******************
 * PRE-PROCESSING *
 *******************/

static int macho_build_segment_32_refs(struct archive_32 *archive) {
   /* link __LINKEDIT segment */
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      union load_command_32 *command = &archive->commands[i];
      if (command->load.cmd == LC_SEGMENT) {
         if (strncmp(command->segment.command.segname, SEG_LINKEDIT,
                     sizeof(command->segment.command.segname)) == 0) {
            struct segment_32 *linkedit_segment = &command->segment;
            
            /* count linkedit commands */
            size_t linkedit_count = 0;
            for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
               union load_command_32 *command = &archive->commands[i];
               if (macho_is_linkedit(command)) {
                  ++linkedit_count;
               }
            }

            /* allocate reference array */
            if ((linkedit_segment->refs =
                 calloc(linkedit_count, sizeof(linkedit_segment->refs[0]))) == NULL) {
               perror("calloc");
               return -1;
            }

            /* populate reference array */
            struct linkedit_data **linkedit_it = (struct linkedit_data **) linkedit_segment->refs;
            for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
               union load_command_32 *command = &archive->commands[i];
               if (macho_is_linkedit(command)) {
                  *linkedit_it++ = &command->linkedit;
               }
            }
         }
      }
   }

   return 0;
}
