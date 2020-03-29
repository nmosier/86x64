#if MACHO_BITS == 32

# define SYMTAB symtab_32
# define DYSYMTAB dysymtab_32
# define SEGMENT segment_32
# define SECTION_WRAPPER section_wrapper_32
# define ARCHIVE archive_32
# define LOAD_COMMAND load_command_32

# define macho_emit_symtab macho_emit_symtab_32
# define macho_emit_dysymtab macho_emit_dysymtab_32
# define macho_emit_segment macho_emit_segment_32
# define macho_emit_archive macho_emit_archive_32
# define macho_emit_load_command macho_emit_load_command_32

#else

# define SYMTAB symtab_64
# define DYSYMTAB dysymtab_64
# define SEGMENT segment_64
# define SECTION_WRAPPER section_wrapper_64
# define ARCHIVE archive_64
# define LOAD_COMMAND load_command_64

# define macho_emit_symtab macho_emit_symtab_64
# define macho_emit_dysymtab macho_emit_dysymtab_64
# define macho_emit_segment macho_emit_segment_64
# define macho_emit_archive macho_emit_archive_64
# define macho_emit_load_command macho_emit_load_command_64

#endif

static int macho_emit_symtab(FILE *f, const struct SYMTAB *symtab);
static int macho_emit_dysymtab(FILE *f, const struct DYSYMTAB *dysymtab);

static int macho_emit_symtab(FILE *f, const struct SYMTAB *symtab) {
   /* emit symtab command */
   if (fwrite_exact(&symtab->command, sizeof(symtab->command), 1, f) < 0) { return -1; }

   /* emit symbol table entries */
   if (fwrite_at(symtab->entries, sizeof(symtab->entries[0]), symtab->command.nsyms, f,
                 symtab->command.symoff) < 0) { return -1; }

   /* emit string table */
   if (fwrite_at(symtab->strtab, 1, symtab->command.strsize, f, symtab->command.stroff) < 0) {
      return -1;
   }

   return 0;
}

static int macho_emit_dysymtab(FILE *f, const struct DYSYMTAB *dysymtab) {
   /* emit dysymtab command */
   if (fwrite_exact(&dysymtab->command, sizeof(dysymtab->command), 1, f) < 0) { return -1; }

   /* emit indirect symbol table */
   if (fwrite_at(dysymtab->indirectsyms, sizeof(dysymtab->indirectsyms[0]),
                 dysymtab->command.nindirectsyms, f, dysymtab->command.indirectsymoff) < 0)
      { return -1; }

   return 0;
}

static int macho_emit_segment(FILE *f, const struct SEGMENT *segment) {
   /* emit segment_command struct */
   if (fwrite_exact(&segment->command, sizeof(segment->command), 1, f) < 0) {
      return -1;
   }

   /* emit section headers */
   uint32_t nsects = segment->command.nsects;
   for (uint32_t i = 0; i < nsects; ++i) {
      const struct SECTION_WRAPPER *sectwr = &segment->sections[i];
      if (fwrite_exact(&sectwr->section, sizeof(sectwr->section), 1, f) < 0) { return -1; }
   }

   /* emit section data */
   for (uint32_t i = 0; i < nsects; ++i) {
      const struct SECTION_WRAPPER *sectwr = &segment->sections[i];
      if (sectwr->section.offset != 0 && sectwr->section.size != 0) {
         if (fwrite_at(sectwr->data, 1, sectwr->section.size, f, sectwr->section.offset) < 0) {
            return -1;
         }
      }
   }

   return 0;
}

int macho_emit_load_command(FILE *f, const union LOAD_COMMAND *command) {
   off_t start_pos;

   /* get command start position */
   if ((start_pos = ftell(f)) < 0) {
      perror("ftell");
      return -1;
   }

   switch (command->load.cmd) {
#if MACHO_BITS == 32
   case LC_SEGMENT:
#else
   case LC_SEGMENT_64:
#endif
      if (macho_emit_segment(f, &command->segment) < 0) { return -1; }
      break;
      
   case LC_SYMTAB:
      if (macho_emit_symtab(f, &command->symtab) < 0) { return -1; }
      break;
      
   case LC_DYSYMTAB:
      if (macho_emit_dysymtab(f, &command->dysymtab) < 0) { return -1; }
      break;
      
   case LC_LOAD_DYLINKER:
      if (macho_emit_dylinker(f, &command->dylinker) < 0) { return -1; }
      break;
      
   case LC_UUID:
      if (fwrite_exact(&command->uuid, sizeof(command->uuid), 1, f) < 0) { return -1; }
      break;
      
   case LC_UNIXTHREAD:
      if (macho_emit_thread(f, &command->thread) < 0) { return -1; }
      break;

   case LC_CODE_SIGNATURE:
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      if (macho_emit_linkedit_data(f, &command->linkedit) < 0) { return -1; }
      break;

   case LC_DYLD_INFO_ONLY:
      if (macho_emit_dyld_info(f, &command->dyld_info) < 0) { return -1; }
      break;

   case LC_VERSION_MIN_MACOSX:
      if (fwrite_exact(&command->version_min, sizeof(command->version_min), 1, f) < 0)
         { return -1; }
      break;

   case LC_SOURCE_VERSION:
      if (fwrite_exact(&command->source_version, sizeof(command->source_version), 1, f) < 0) {
         return -1;
      }
      break;

   case LC_MAIN:
      if (fwrite_exact(&command->entry_point, sizeof(command->entry_point), 1, f) < 0) {
         return -1;
      }
      break;

   case LC_ID_DYLIB:
   case LC_LOAD_DYLIB:
      if (macho_emit_dylib(f, &command->dylib) < 0) { return -1; }
      break;

   case LC_BUILD_VERSION:
      if (macho_emit_build_version(f, &command->build_version) < 0) { return -1; }
      break;

   default:
      fprintf(stderr, "macho_emit_load_command_32: unsupported command 0x%x\n", command->load.cmd);
      return -1;
   }

   /* seek next cmdsize bytes */
   if (fseek(f, start_pos + command->load.cmdsize, SEEK_SET) < 0) {
      perror("fseek");
      return -1;
   }
   
   return 0;
}

int macho_emit_archive(FILE *f, const struct ARCHIVE *archive) {
   /* emit header */
   if (fwrite_exact(&archive->header, sizeof(archive->header), 1, f) < 0) {
      return -1;
   }

   /* emit commands */
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      if (macho_emit_load_command(f, &archive->commands[i]) < 0) {
         return -1;
      }
   }

   return 0;
}


#undef SYMTAB
#undef DYSYMTAB
#undef SEGMENT
#undef SECTION_WRAPPER
#undef ARCHIVE
#undef LOAD_COMMAND

#undef macho_emit_symtab
#undef macho_emit_dysymtab
#undef macho_emit_segment
#undef macho_emit_archive
#undef macho_emit_load_command

#undef MACHO_BITS
