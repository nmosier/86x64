/* macho-parse C template file */

#include "macho.h"
#include "macho-util.h"

/* MACHO_BITS -- bits (32 or 64) */

/* function names */

/* define struct names */

/* declarations */
#if MACHO_BITS == 32

# define NLIST nlist
# define SYMTAB symtab_32
# define DYSYMTAB dysymtab_32
# define SEGMENT segment_32
# define SECTION_WRAPPER section_wrapper_32
# define LOAD_COMMAND load_command_32
# define ARCHIVE archive_32
# define macho_parse_symtab macho_parse_symtab_32
# define macho_parse_dysymtab macho_parse_dysymtab_32
# define macho_parse_segment macho_parse_segment_32
# define macho_parse_load_command macho_parse_load_command_32
# define macho_parse_archive macho_parse_archive_32

# define MACHO_SIZE_T uint32_t

#else

# define NLIST nlist_64
# define SYMTAB symtab_64
# define DYSYMTAB dysymtab_64
# define SEGMENT segment_64
# define SECTION_WRAPPER section_wrapper_64
# define LOAD_COMMAND load_command_64
# define ARCHIVE archive_64
# define macho_parse_symtab macho_parse_symtab_64
# define macho_parse_dysymtab macho_parse_dysymtab_64
# define macho_parse_segment macho_parse_segment_64
# define macho_parse_load_command macho_parse_load_command_64
# define macho_parse_archive macho_parse_archive_64
# define MACHO_SIZE_T uint64_t

#endif

static int macho_parse_symtab(FILE *f, struct SYMTAB *symtab);
static int macho_parse_dysymtab(FILE *f, struct DYSYMTAB *dysymtab);
static int macho_parse_segment(FILE *f, struct SEGMENT *segment);
int macho_parse_archive(FILE *f, const uint32_t *magic, struct ARCHIVE *archive);

static int macho_parse_symtab(FILE *f, struct SYMTAB *symtab) {
   /* parse symtab command */
   if (fread_exact(AFTER(symtab->command.cmdsize),
                   STRUCT_REM(symtab->command, cmdsize), 1, f) < 0) {
      return -1;
   }

   /* parse symtab entries */
   if (symtab->command.nsyms == 0) {
      symtab->entries = NULL;
   } else {
      /* NOTE: Doesn't parse struct nlist entries intelligently; just loads blindly.
       * However, this shouldn't be a problem unless the string table is modified. 
       */
      if ((symtab->entries = calloc(symtab->command.nsyms, sizeof(symtab->entries[0]))) == NULL) {
         perror("calloc");
         return -1;
      }
      if (fread_at(symtab->entries, sizeof(symtab->entries[0]), symtab->command.nsyms, f,
                   symtab->command.symoff) < 0) { return -1; }
   }

   /* parse string table */
   if (symtab->command.strsize == 0) {
      symtab->strtab = NULL;
   } else {
      if (fseek(f, symtab->command.stroff, SEEK_SET) < 0) {
         perror("fseek");
         return -1;
      }
      if ((symtab->strtab = fmread(1, symtab->command.strsize, f)) == NULL) {
         return -1;
      }
   }

   // DEBUG
   printf("SYMTAB{");
   for (uint32_t i = 0; i < symtab->command.nsyms; ++i) {
      struct NLIST *entry = &symtab->entries[i];
      printf("\t{name='%s',value=0x%llx,desc=0x%x,type=0x%x,sect=0x%x}\n",
             &symtab->strtab[entry->n_un.n_strx],
             (macho_addr_t) entry->n_value,
             entry->n_desc,
             entry->n_type,
             entry->n_sect
             );
   }
   printf("}\n");
   
   return 0;
}


static int macho_parse_dysymtab(FILE *f, struct DYSYMTAB *dysymtab) {
   /* parse dysymtab command */
   if (fread_exact(AFTER(dysymtab->command.cmdsize),
                   STRUCT_REM(dysymtab->command, cmdsize), 1, f) < 0) {
      return -1;
   }

   /* parse data */
   if (dysymtab->command.ntoc) {
      fprintf(stderr,
              "macho_parse_dysymtab: parsing table of contents not supported\n");
      return -1;
   }
   if (dysymtab->command.nmodtab) {
      fprintf(stderr,
              "macho_parse_dysymtab: parsing module table not supported\n");
      return -1;
   }
   if (dysymtab->command.nextrefsyms) {
      fprintf(stderr,
              "macho_parse_dysymtab: parsing referenced symbol table not supported\n");
      return -1;
   }
   if (dysymtab->command.nextrel) {
      fprintf(stderr,
              "macho_parse_dysymtab: parsing external relocation entries not supported\n");
      return -1;
   }
   if (dysymtab->command.nlocrel) {
      fprintf(stderr,
              "macho_parse_dysymtab: parsing local relocation entries not supported\n");
      return -1;
   }

   /* parse indirect syms */
   if ((dysymtab->indirectsyms = fmread_at(sizeof(dysymtab->indirectsyms[0]),
                                           dysymtab->command.nindirectsyms, f,
                                           dysymtab->command.indirectsymoff)) == NULL)
      { return -1; }

   return 0;
}

/* NOTE: Load command prefix already parsed. 
 * NOTE: Leaves file offset in indeterminate state. */
static int macho_parse_segment(FILE *f, struct SEGMENT *segment) {
   /* parse segment command */
   if (fread_exact(AFTER(segment->command.cmdsize),
                   STRUCT_REM(segment->command, cmdsize), 1, f) < 0) {
      return -1;
   }

   /* parse sections */
   if (segment->command.nsects == 0) {
      segment->sections = NULL;
   } else {
      uint32_t nsects = segment->command.nsects;

      /* allocate sections array */
      if ((segment->sections = calloc(nsects, sizeof(segment->sections[0]))) == NULL) {
         perror("calloc");
         return -1;
      }

      /* parse sections array */
      for (uint32_t i = 0; i < nsects; ++i) {
         struct SECTION_WRAPPER *sectwr = &segment->sections[i];
         
         /* read section struct */
         if (fread_exact(&sectwr->section, sizeof(sectwr->section), 1, f) < 0) { return -1; }

         /* guard against unsupported features */
         if (sectwr->section.nreloc != 0) {
            fprintf(stderr, "macho_parse_segment: relocation entries not supported\n");
            return -1;
         }
      }

      /* do section_wrapper initialization work */
      for (uint32_t i = 0; i < nsects; ++i) {
         struct SECTION_WRAPPER *sectwr = &segment->sections[i];
         if (sectwr->section.size == 0 || sectwr->section.offset == 0) {
            sectwr->data = NULL;
         } else {
            /* seek to data */
            if (fseek(f, sectwr->section.offset, SEEK_SET) < 0) {
               perror("fseek");
               return -1;
            }
            
            /* parse data */
            if ((sectwr->data = fmread(1, sectwr->section.size, f)) == NULL) { return -1; }
         }
      }
   }

   return 0;
}

int macho_parse_load_command(FILE *f, union LOAD_COMMAND *command) {
   off_t offset;

   /* save offset */
   if ((offset = ftell(f)) < 0) {
      perror("ftell");
      return -1;
   }
   
   /* parse shared load command preamble */
   if (fread_exact(&command->load, sizeof(command->load), 1, f) < 0) { return -1; }

   switch (command->load.cmd) {
#if MACHO_BITS == 32
   case LC_SEGMENT:
      if (macho_parse_segment_32(f, &command->segment) < 0) { return -1; }
      break;
#else
   case LC_SEGMENT_64:
      if (macho_parse_segment_64(f, &command->segment) < 0) { return -1; }
      break;
#endif
   case LC_SYMTAB:
      if (macho_parse_symtab(f, &command->symtab) < 0) { return -1; }
      break;

   case LC_DYSYMTAB:
      if (macho_parse_dysymtab(f, &command->dysymtab) < 0) { return -1; }
      break;

   case LC_LOAD_DYLINKER:
      if (macho_parse_dylinker(f, &command->dylinker) < 0) { return -1; }
      break;

   case LC_UUID:
      if (macho_parse_uuid(f, &command->uuid) < 0) { return -1; }
      break;

   case LC_UNIXTHREAD:
      if (macho_parse_thread(f, &command->thread) < 0) { return -1; }
      break;

   case LC_CODE_SIGNATURE:
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      if (macho_parse_linkedit(f, &command->linkedit) < 0) { return -1; }
      break;

   case LC_DYLD_INFO:
   case LC_DYLD_INFO_ONLY:
      if (macho_parse_dyld_info(f, &command->dyld_info) < 0) { return -1; }
      break;

   case LC_VERSION_MIN_MACOSX:
      if (fread_exact(AFTER(command->version_min.cmdsize), 1,
                      STRUCT_REM(command->version_min, cmdsize), f) < 0) { return -1; }
      break;

   case LC_SOURCE_VERSION:
      if (fread_exact(AFTER(command->source_version.cmdsize), 1,
                      STRUCT_REM(command->source_version, cmdsize), f) < 0) { return -1; }
      break;

   case LC_MAIN:
      if (fread_exact(AFTER(command->entry_point.cmdsize), 1,
                      STRUCT_REM(command->entry_point, cmdsize), f) < 0) { return -1; }

      // DEBUG
      printf("LC_MAIN={.entryoff=0x%llx}\n", command->entry_point.entryoff);
      
      break;

   case LC_ID_DYLIB:
   case LC_LOAD_DYLIB:
      if (macho_parse_dylib(f, &command->dylib) < 0) { return -1; }
      break;

   case LC_BUILD_VERSION:
      if (macho_parse_build_version(f, &command->build_version) < 0) { return -1; }
      break;

   default:
      fprintf(stderr, "macho_parse_load_command: unrecognized load command type 0x%x\n",
              command->load.cmd);
      return -1;
   }

   /* restore offset */
   if (fseek(f, offset + command->load.cmdsize, SEEK_SET) < 0) {
      perror("fseek");
      return -1;
   }

   return 0;
}


int macho_parse_archive(FILE *f, const uint32_t *magic, struct ARCHIVE *archive) {
   /* parse header */
   void *hdr_start;
   size_t hdr_size;

   if (magic) {
      archive->header.magic = *magic;
      hdr_start = AFTER(archive->header.magic);
      hdr_size = STRUCT_REM(archive->header, magic);
   } else {
      hdr_start = &archive->header;
      hdr_size = sizeof(archive->header);
   }

   if (fread_exact(hdr_start, hdr_size, 1, f) < 0) {
      return -1;
   }

   /* allocate commands */
   if ((archive->commands = calloc(archive->header.ncmds, sizeof(*archive->commands))) == NULL) {
      perror("calloc");
      return -1;
   }
   
   /* parse commands */
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      union LOAD_COMMAND *command = &archive->commands[i];
      if (macho_parse_load_command(f, command) < 0) {
         return -1;
      }
   }

   return 0;
}


#undef NLIST
#undef SYMTAB
#undef DYSYMTAB
#undef SEGMENT
#undef SECTION_WRAPPER
#undef LOAD_COMMAND
#undef ARCHIVE

#undef macho_parse_symtab
#undef macho_parse_dysymtab
#undef macho_parse_segment
#undef macho_parse_load_command
#undef macho_parse_archive

#undef MACHO_SIZE_T

#undef MACHO_BITS
