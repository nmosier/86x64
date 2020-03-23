#include <string.h>

#include "macho-parse.h"
#include "macho-util.h"
#include "macho-build.h"
#include "macho-sizeof.h"
#include "util.h"

static int macho_build_segment_32_LINKEDIT(struct segment_32 *segment, struct build_info *info);


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
      
   case LC_SYMTAB:
      return macho_build_symtab_32(&command->symtab, info);

   case LC_DYSYMTAB:
      return macho_build_dysymtab_32(&command->dysymtab, info);

   case LC_LOAD_DYLINKER:
   case LC_UUID:
   case LC_UNIXTHREAD:
      /* nothing to adjust */
      return 0;

      /* linkedit commands are adjusted by __LINKEDIT segment */
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      return 0;

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
   /* check for specific segment rules */
   if (strncmp(segment->command.segname, SEG_LINKEDIT, sizeof(segment->command.segname)) == 0) {
      return macho_build_segment_32_LINKEDIT(segment, info);
   }
   
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

   /* update section information */
   macho_off_t dataoff = info->dataoff;
   macho_addr_t vmaddr = info->vmaddr + dataoff % PAGESIZE;
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      struct section *section = &segment->sections[i].section;

      /* update offsets */
      dataoff = ALIGN_UP(dataoff, 1 << section->align);
      section->offset = dataoff;
      dataoff += section->size;

      /* update virtual address */
      vmaddr = ALIGN_UP(vmaddr, 1 << section->align);
      section->addr = vmaddr;
      vmaddr += section->size;

#if 0
      // DEBUG
      if (strncmp(section->segname, SEG_TEXT, sizeof(section->segname)) == 0) {
         /* override */
         section->addr = 0x1fff;
         section->offset = 0xfff;
      }
#endif
   }
   
   /* update build info */
   info->dataoff = MAX(segment_start + segment_minsize_aligned, info->dataoff);
   info->vmaddr += vmsize;
   
   return 0;
}

int macho_build_segment_32_LINKEDIT(struct segment_32 *segment, struct build_info *info) {
   /* check for unsupported features */
   if (segment->command.nsects) {
      fprintf(stderr,
              "macho_build_segment_32_LINKEDIT: __LINKEDIT segment with sections not supported\n");
      return -1;
   }

   struct archive_32 *archive = &info->archive->archive_32;
   uint32_t ncmds = archive->header.ncmds;

   macho_off_t start_offset = ALIGN_DOWN(info->dataoff, PAGESIZE);
   segment->command.vmaddr = info->vmaddr;
   macho_addr_t vmaddr = segment->command.vmaddr + info->dataoff % PAGESIZE;
   macho_off_t dataoff = info->dataoff;
   segment->command.fileoff = dataoff;

   for (uint32_t i = 0; i < ncmds; ++i) {
      union load_command_32 *command = &archive->commands[i];
      struct linkedit_data *linkedit;
      if ((linkedit = macho_linkedit(command))) {
         linkedit->command.dataoff = dataoff;
         dataoff += linkedit->command.datasize;
      } else if (command->load.cmd == LC_SYMTAB) {
         struct symtab_32 *symtab = &command->symtab;
         symtab->command.symoff = dataoff;
         dataoff += symtab->command.nsyms * sizeof(symtab->entries[0]);
         symtab->command.stroff = dataoff;
         dataoff += symtab->command.strsize;
      }
   }

   segment->command.filesize = dataoff - segment->command.fileoff;
   segment->command.vmsize = MAX(ALIGN_UP(segment->command.filesize, PAGESIZE), PAGESIZE);
   info->vmaddr += segment->command.vmsize;
   info->dataoff = dataoff;
   
   // DEBUG
   printf("__LINKEDIT={.size=0x%x}\n", segment->command.filesize);
   printf("dataoff=0x%llx,datatend=0x%llx\n", start_offset, info->dataoff);

   return 0;
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
   if (dysymtab->command.nindirectsyms) {
      fprintf(stderr,
              "macho_parse_dysymtab_32: parsing indirect symbol table not supported\n");
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
   
   return 0;
}
