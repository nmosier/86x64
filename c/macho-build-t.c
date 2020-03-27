#if MACHO_BITS == 32

# define SYMTAB symtab_32
# define DYSYMTAB dysymtab_32
# define SEGMENT segment_32
# define SECTION_WRAPPER section_wrapper_32
# define ARCHIVE archive_32
# define LOAD_COMMAND load_command_32
# define SECTION section
# define NLIST nlist

# define macho_build_symtab           macho_build_symtab_32
# define macho_build_dysymtab         macho_build_dysymtab_32
# define macho_build_segment          macho_build_segment_32
# define macho_build_archive          macho_build_archive_32
# define macho_build_load_command     macho_build_load_command_32
# define macho_build_segment_LINKEDIT macho_build_segment_32_LINKEDIT
# define macho_build_dyld_info        macho_build_dyld_info_32
# define macho_build_segment_BSS      macho_build_segment_BSS_32

# define macho_find_load_command      macho_find_load_command_32
# define macho_linkedit               macho_linkedit_32
# define macho_sizeof_load_command    macho_sizeof_load_command_32
# define macho_vmsizeof_segment       macho_vmsizeof_segment_32

# define MACHO_SIZE_T uint32_t
# define MACHO_ADDR_T uint32_t

# define build_info build_info_32

#else

# define SYMTAB symtab_64
# define DYSYMTAB dysymtab_64
# define SEGMENT segment_64
# define SECTION_WRAPPER section_wrapper_64
# define ARCHIVE archive_64
# define LOAD_COMMAND load_command_64
# define SECTION section_64
# define NLIST nlist_64

# define macho_build_symtab           macho_build_symtab_64
# define macho_build_dysymtab         macho_build_dysymtab_64
# define macho_build_segment          macho_build_segment_64
# define macho_build_archive          macho_build_archive_64
# define macho_build_load_command     macho_build_load_command_64
# define macho_build_segment_LINKEDIT macho_build_segment_64_LINKEDIT
# define macho_build_dyld_info        macho_build_dyld_info_64
# define macho_build_segment_BSS      macho_build_segment_BSS_64
# define macho_vmsizeof_segment       macho_vmsizeof_segment_64

# define macho_find_load_command      macho_find_load_command_64
# define macho_linkedit               macho_linkedit_64
# define macho_sizeof_load_command    macho_sizeof_load_command_64

# define build_info build_info_64

# define MACHO_SIZE_T uint64_t
# define MACHO_ADDR_T uint64_t

#endif

#include <assert.h>

#include "macho.h"
#include "macho-util.h"
#include "macho-build.h"

int macho_build_segment_LINKEDIT(struct SEGMENT *segment, struct build_info *info);
int macho_build_symtab(struct SYMTAB *symtab, struct build_info *info);
int macho_build_dysymtab(struct DYSYMTAB *dysymtab, struct build_info *info);
int macho_build_load_command(union LOAD_COMMAND *command, struct build_info *info);
int macho_build_dyld_info(struct dyld_info *dyld_info, struct build_info *info);

MACHO_SIZE_T macho_vmsizeof_segment(const struct SEGMENT *segment);

/* NOTES:
 *  - Each segment needs to start on a page bounary, i.e. its file offset must be a multiple of
 *    0x1000.
 * QUESTIONS:
 *  - Do the first segments need to be mapped for an entire page if they only take up a small 
 *    portion of it?
 */
int macho_build_segment(struct SEGMENT *segment, struct build_info *info) {
   /* check for specific segment rules */
   if (strncmp(segment->command.segname, SEG_LINKEDIT, sizeof(segment->command.segname)) == 0) {
      return macho_build_segment_LINKEDIT(segment, info);
   }

#if 0
   /* get max alignment */
   uint32_t section_maxalign = 0;
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      section_maxalign = MAX(segment->sections[i].section.align, section_maxalign);
   }
   
   /* recompute size required for all sections */
   /* NOTE: This is conservative in alignment requirements. */
   MACHO_SIZE_T segment_minsize = 0;
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      segment_minsize += ALIGN_UP(segment->sections[i].section.size, 1 << section_maxalign);
   }
   MACHO_SIZE_T segment_minsize_aligned = ALIGN_UP(segment_minsize, PAGESIZE);
#else
   MACHO_SIZE_T segment_minsize_aligned =
      ALIGN_UP(macho_vmsizeof_segment(segment) + info->dataoff % PAGESIZE, PAGESIZE);
#endif

   /* allocate data region for segment */
   MACHO_SIZE_T segment_start = ALIGN_DOWN(info->dataoff, PAGESIZE);

   /* update segment command information */
   segment->command.fileoff = segment_start;
   segment->command.filesize = segment_minsize_aligned;
   segment->adiff = info->vmaddr - segment->command.vmaddr; // segment->command.vmaddr - info->vmaddr;
   segment->command.vmaddr = info->vmaddr;
   MACHO_SIZE_T vmsize = MAX(PAGESIZE, segment_minsize_aligned);
   segment->command.vmsize = vmsize;

   /* update section information */
   MACHO_SIZE_T dataoff = info->dataoff;
   MACHO_ADDR_T vmaddr = info->vmaddr + dataoff % PAGESIZE;
   for (uint32_t i = 0; i < segment->command.nsects; ++i, ++info->nsects) {
      struct SECTION_WRAPPER *sectwr = &segment->sections[i];
      struct SECTION *section = &sectwr->section;

      sectwr->adiff = - section->addr;
      sectwr->odiff = - section->offset;
      
      /* function starts data */
      struct linkedit_data *fnstarts = NULL;
      MACHO_ADDR_T *fnstarts_addrs;
      MACHO_SIZE_T fnstarts_naddrs;

      /* entry point data */
      struct entry_point_command *entry_point = NULL;

      /* check if __text -- if so, then update function starts */
      if (strncmp(section->sectname, SECT_TEXT, sizeof(section->sectname)) == 0) {
         /* find function starts command */
         union LOAD_COMMAND *command;
         if ((command = macho_find_load_command(LC_FUNCTION_STARTS, info->archive))) {
            fnstarts = &command->linkedit;
            fnstarts_addrs = fnstarts->data;
            fnstarts_naddrs = fnstarts->command.datasize / sizeof(fnstarts_addrs[0]);
            for (MACHO_SIZE_T i = 0; i < fnstarts_naddrs; ++i) {
               fnstarts_addrs[i] -= section->addr;
            }
         }

         /* find entry point command */
         if ((command = macho_find_load_command(LC_MAIN, info->archive))) {
            // DEBUG
            printf("old entryoff = 0x%llx\n", command->entry_point.entryoff);
            
            entry_point = &command->entry_point;
            entry_point->entryoff -= section->offset;
         }
      }

      /* update offsets */
      if (section->offset != 0) {
         dataoff = ALIGN_UP(dataoff, 1 << section->align);
         section->offset = dataoff;
         dataoff += section->size;
      }

      /* update virtual address */
      vmaddr = ALIGN_UP(vmaddr, 1 << section->align);
      section->addr = vmaddr;
      vmaddr += section->size;

      /* patch function starts */
      if (fnstarts) {
         for (MACHO_SIZE_T i = 0; i < fnstarts_naddrs; ++i) {
            fnstarts_addrs[i] += section->addr;
         }
      }

      /* patch entry point command */
      if (entry_point) {
         entry_point->entryoff += section->offset;

         // DEBUG
         printf("new entryoff = 0x%llx\n", entry_point->entryoff);
      }

      // vmaddr_diff += section->addr;
      sectwr->adiff += section->addr;
      sectwr->odiff += section->offset;

      /* patch symbols */
      union LOAD_COMMAND *symtab_tmp;
      if ((symtab_tmp = macho_find_load_command(LC_SYMTAB, info->archive))) {
         struct SYMTAB *symtab = &symtab_tmp->symtab;
         for (uint32_t j = 0; j < symtab->command.nsyms; ++j) {
            struct NLIST *entry = &symtab->entries[j];
            if (entry->n_sect == info->nsects + 1) {
               // DEBUG
               printf("nlist{name='%s',value=0x%llx}\n", symtab->strtab + entry->n_un.n_strx,
                      (uint64_t) entry->n_value);
               
               /* patch symbol address */
               entry->n_value += sectwr->adiff; // vmaddr_diff;

               // DEBUG
               printf("nlist{name='%s',value=0x%llx}\n", symtab->strtab + entry->n_un.n_strx,
                      (uint64_t) entry->n_value);
               
            }
         }

         // DEBUG
         printf("[built symtab]\n");
      }

      /* patch thread states */
      union LOAD_COMMAND *thread_tmp;
      if ((thread_tmp = macho_find_load_command(LC_UNIXTHREAD, info->archive))) {
         struct thread_list *it = thread_tmp->thread.entries;
         while (it) {
            switch (it->entry.header.flavor) {
            case x86_THREAD_STATE32:
               it->entry.x86_thread.uts.ts32.__eip += sectwr->adiff;
               break;
               
            default:
               fprintf(stderr, "macho_build_segment_32: thread state flavor 0x%x unsupported\n",
                       it->entry.header.flavor);
               return -1;
            }
            
            it = it->next;
         }
      }

   }
   
   /* update build info */
   info->dataoff = MAX(segment_start + segment_minsize_aligned, info->dataoff);
   info->vmaddr += vmsize;

#if 1
   /* consistency checks */
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      struct SECTION *section = &segment->sections[i].section;
      assert(section->addr >= segment->command.vmaddr);
      assert(section->addr + section->size <= segment->command.vmaddr + segment->command.vmsize);
   }
#endif
   
   return 0;
}


int macho_build_segment_LINKEDIT(struct SEGMENT *segment, struct build_info *info) {
   /* check for unsupported features */
   if (segment->command.nsects) {
      fprintf(stderr,
              "macho_build_segment_LINKEDIT: __LINKEDIT segment with sections not supported\n");
      return -1;
   }

   struct ARCHIVE *archive = info->archive;
   uint32_t ncmds = archive->header.ncmds;
   
   macho_off_t start_offset = ALIGN_DOWN(info->dataoff, PAGESIZE);
   segment->command.vmaddr = info->vmaddr;
   MACHO_ADDR_T vmaddr = segment->command.vmaddr + info->dataoff % PAGESIZE;
   // macho_off_t dataoff = info->dataoff;
   segment->command.fileoff = info->dataoff;

   for (uint32_t i = 0; i < ncmds; ++i) {
      union LOAD_COMMAND *command = &archive->commands[i];
      struct linkedit_data *linkedit;
      if ((linkedit = macho_linkedit(command))) {
         linkedit->command.dataoff = info->dataoff;
         info->dataoff += linkedit->command.datasize;
      } else {
         switch (command->load.cmd) {
         case LC_SYMTAB:
            if (macho_build_symtab(&command->symtab, info) < 0) { return -1; }
            break;
         
         case LC_DYLD_INFO_ONLY:
         case LC_DYLD_INFO:
            if (macho_build_dyld_info(&command->dyld_info, info) < 0) { return -1; }
            break;
            
         case LC_DYSYMTAB:
            if (macho_build_dysymtab(&command->dysymtab, info) < 0) { return -1; }
            break;

         default: break;
         }
      }
   }

   segment->command.filesize = info->dataoff - segment->command.fileoff;
   segment->command.vmsize = MAX(ALIGN_UP(segment->command.filesize, PAGESIZE), PAGESIZE);
   info->vmaddr += segment->command.vmsize;
   
   // DEBUG
   printf("__LINKEDIT={.size=0x%llx}\n", (uint64_t) segment->command.filesize);
   printf("dataoff=0x%llx,datatend=0x%llx\n", (uint64_t) start_offset, (uint64_t) info->dataoff);

   return 0;
}

#if 0
/* Sections in the BSS segment don't require any storage in the object file,
 * but they still need to be assigned virtual addresses.
 */
int macho_build_segment_BSS(struct SEGMENT *bss, struct build_info *info) {
   uint32_t nsects = bss->command.nsects;
   
   /* compute size of all sections */
   bss->command.vmsize = macho_vmsizeof_segment(bss);

   /* assign address range */
   bss->command.vmaddr = info->vmaddr;
   info->vmaddr += bss->command.vmsize;

   /* assign address subranges to segments */
   MACHO_ADDR_T addr = bss->command.vmaddr;
   for (uint32_t i = 0; i < bss->command.nsects; ++i) {
      struct SECTION *section = &bss->sections[i].section;
      addr = ALIGN_UP(addr, 1 << section->align);
      section->addr = addr;
      addr += section->size;
      section->offset = 0;
   }

   return 0;
}
#endif


int macho_build_symtab(struct SYMTAB *symtab, struct build_info *info) {
   /* allocate data region */
   symtab->command.symoff = info->dataoff;
   info->dataoff += symtab->command.nsyms * sizeof(symtab->entries[0]);
   symtab->command.stroff = info->dataoff;
   info->dataoff += symtab->command.strsize;

   // DEBUG
   printf("symtab dataoff = 0x%x; symtab dataend = 0x%llx\n", symtab->command.symoff,
          (uint64_t) info->dataoff);
   
   return 0;
}

int macho_build_dysymtab(struct DYSYMTAB *dysymtab, struct build_info *info) {
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


int macho_build_load_command(union LOAD_COMMAND *command, struct build_info *info) {
   switch (command->load.cmd) {
#if MACHO_BITS == 32
   case LC_SEGMENT:
#else
   case LC_SEGMENT_64:
#endif
      return macho_build_segment(&command->segment, info);
            
   case LC_LOAD_DYLINKER:
   case LC_UUID:
   case LC_UNIXTHREAD:
   case LC_VERSION_MIN_MACOSX:
   case LC_SOURCE_VERSION:
   case LC_LOAD_DYLIB:
   case LC_BUILD_VERSION:
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

int macho_build_archive(struct ARCHIVE *archive, struct build_info *info) {
   /* recompute size of commands */
   size_t sizeofcmds = 0;
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      sizeofcmds += macho_sizeof_load_command(&archive->commands[i]);
   }
   archive->header.sizeofcmds = sizeofcmds;
   
   /* update data offset */
   info->dataoff += sizeofcmds + sizeof(archive->header);

   /* build each command */
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      if (macho_build_load_command(&archive->commands[i], info) < 0) {
         return -1;
      }
   }

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



#undef SYMTAB
#undef DYSYMTAB
#undef SEGMENT
#undef SECTION_WRAPPER
#undef ARCHIVE
#undef LOAD_COMMAND
#undef SECTION
#undef NLIST

#undef macho_build_symtab
#undef macho_build_dysymtab
#undef macho_build_segment
#undef macho_build_archive
#undef macho_build_load_command
#undef macho_build_segment_LINKEDIT
#undef macho_build_dyld_info
#undef macho_build_segment_BSS

#undef macho_find_load_command
#undef macho_linkedit
#undef macho_sizeof_load_command
#undef macho_vmsizeof_segment

#undef MACHO_SIZE_T
#undef MACHO_ADDR_T

#undef build_info

#undef MACHO_BITS
