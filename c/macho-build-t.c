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

# define MACHO_SIZE_T uint32_t
# define MACHO_ADDR_T uint32_t

#else

# define SYMTAB symtab_64
# define DYSYMTAB dysymtab_64
# define SEGMENT segment_64
# define SECTION_WRAPPER section_wrapper_64
# define ARCHIVE archive_64
# define LOAD_COMMAND load_command_64
# define SECTION section_64
# define NLIST nlist_64

# define macho_build_symtab           macho_emit_symtab_64
# define macho_build_dysymtab         macho_emit_dysymtab_64
# define macho_build_segment          macho_emit_segment_64
# define macho_build_archive          macho_emit_archive_64
# define macho_build_load_command     macho_emit_load_command_64
# define macho_build_segment_LINKEDIT macho_build_segment_64_LINKEDIT

# define MACHO_SIZE_T uint64_t
# define MACHO_ADDR_T uint64_t

#endif


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

   /* allocate data region for segment */
   MACHO_SIZE_T segment_start = ALIGN_DOWN(info->dataoff, PAGESIZE);

   /* update segment command information */
   segment->command.fileoff = segment_start;
   segment->command.filesize = segment_minsize_aligned;
   segment->adiff = segment->command.vmaddr - info->vmaddr;
   segment->command.vmaddr = info->vmaddr;
   MACHO_SIZE_T vmsize = MAX(PAGESIZE, segment_minsize_aligned);
   segment->command.vmsize = vmsize;

   /* update section information */
   MACHO_SIZE_T dataoff = info->dataoff;
   MACHO_ADDR_T vmaddr = info->vmaddr + dataoff % PAGESIZE;
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
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
         if ((command = macho_find_load_command_32(LC_FUNCTION_STARTS,
                                                   &info->archive->archive_32))) {
            fnstarts = &command->linkedit;
            fnstarts_addrs = fnstarts->data;
            fnstarts_naddrs = fnstarts->command.datasize / sizeof(fnstarts_addrs[0]);
            for (MACHO_SIZE_T i = 0; i < fnstarts_naddrs; ++i) {
               fnstarts_addrs[i] -= section->addr;
            }
         }

         /* find entry point command */
         if ((command = macho_find_load_command_32(LC_MAIN, &info->archive->archive_32))) {
            // DEBUG
            printf("old entryoff = 0x%llx\n", command->entry_point.entryoff);
            
            entry_point = &command->entry_point;
            entry_point->entryoff -= section->offset;
         }
      }

      /* update offsets */
      dataoff = ALIGN_UP(dataoff, 1 << section->align);
      section->offset = dataoff;
      dataoff += section->size;

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
      if ((symtab_tmp = macho_find_load_command_32(LC_SYMTAB, &info->archive->archive_32))) {
         struct SYMTAB *symtab = &symtab_tmp->symtab;
         for (uint32_t j = 0; j < symtab->command.nsyms; ++j) {
            struct NLIST *entry = &symtab->entries[j];
            if (entry->n_sect == i + 1) {
               /* patch symbol address */
               entry->n_value += sectwr->adiff; // vmaddr_diff;
            }
         }
      }

      /* patch thread states */
      union LOAD_COMMAND *thread_tmp;
      if ((thread_tmp = macho_find_load_command_32(LC_UNIXTHREAD, &info->archive->archive_32))) {
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
   // macho_off_t dataoff = info->dataoff;
   segment->command.fileoff = info->dataoff;

   for (uint32_t i = 0; i < ncmds; ++i) {
      union load_command_32 *command = &archive->commands[i];
      struct linkedit_data *linkedit;
      if ((linkedit = macho_linkedit(command))) {
         linkedit->command.dataoff = info->dataoff;
         info->dataoff += linkedit->command.datasize;
      } else {
         switch (command->load.cmd) {
         case LC_SYMTAB:
            if (macho_build_symtab_32(&command->symtab, info) < 0) { return -1; }
            break;
         
         case LC_DYLD_INFO_ONLY:
         case LC_DYLD_INFO:
            if (macho_build_dyld_info(&command->dyld_info, info) < 0) { return -1; }
            break;
            
         case LC_DYSYMTAB:
            if (macho_build_dysymtab_32(&command->dysymtab, info) < 0) { return -1; }
            break;

         default: break;
         }
      }
   }

   segment->command.filesize = info->dataoff - segment->command.fileoff;
   segment->command.vmsize = MAX(ALIGN_UP(segment->command.filesize, PAGESIZE), PAGESIZE);
   info->vmaddr += segment->command.vmsize;
   
   // DEBUG
   printf("__LINKEDIT={.size=0x%x}\n", segment->command.filesize);
   printf("dataoff=0x%llx,datatend=0x%llx\n", start_offset, info->dataoff);

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

#undef MACHO_SIZE_T
#undef MACHO_ADDR_T

#undef MACHO_BITS
