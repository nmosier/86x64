/* macho-parse C template file */

#include "macho.h"
#include "macho-util.h"

/* MACHO_BITS -- bits (32 or 64) */

/* function names */

/* define struct names */
#if MACHO_BITS == 32
# define NLIST nlist
# define SYMTAB symtab_32
# define DYSYMTAB dysymtab_32
# define SEGMENT segment_32
# define SECTION_WRAPPER section_wrapper_32
# define macho_parse_symtab macho_parse_symtab_32
# define macho_parse_dysymtab macho_parse_dysymtab_32
# define macho_parse_segment macho_parse_segment_32
# define MACHO_SIZE_T uint32_t
#else
# define NLIST nlist_64
# define SYMTAB symtab_64
# define DYSYMTAB dysymtab_64
# define SEGMENT segment_64
# define SECTION_WRAPPER section_wrapper_64
# define macho_parse_symtab macho_parse_symtab_64
# define macho_parse_dysymtab macho_parse_dysymtab_64
# define macho_parse_segment macho_parse_segment_64
# define MACHO_SIZE_T uint64_t
#endif

/* declarations */
static int macho_parse_symtab(FILE *f, struct SYMTAB *symtab);
static int macho_parse_dysymtab(FILE *f, struct DYSYMTAB *dysymtab);
static int macho_parse_segment(FILE *f, struct SEGMENT *segment);

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
            fprintf(stderr, "macho_parse_segment_32: relocation entries not supported\n");
            return -1;
         }
      }

      /* do section_wrapper initialization work */
      for (uint32_t i = 0; i < nsects; ++i) {
         struct SECTION_WRAPPER *sectwr = &segment->sections[i];
         MACHO_SIZE_T data_size = sectwr->section.size;
         if (data_size == 0) {
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


#undef NLIST
#undef SYMTAB
#undef DYSYMTAB
#undef SEGMENT
#undef SECTION_WRAPPER

#undef macho_parse_symtab
#undef macho_parse_dysymtab
#undef macho_parse_segment

#undef MACHO_SIZE_T

#undef MACHO_BITS
