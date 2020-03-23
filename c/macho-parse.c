#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "macho-parse.h"
#include "macho-util.h"
#include "util.h"

static int macho_parse_segment_32(FILE *f, struct segment_32 *segment);
static int macho_parse_symtab_32(FILE *f, struct symtab_32 *symtab);
static int macho_parse_dysymtab_32(FILE *f, struct dysymtab_32 *dysymtab);
static int macho_parse_dylinker(FILE *f, struct dylinker *dylinker);
static int macho_parse_uuid(FILE *f, struct uuid_command *uuid);
static int macho_parse_thread(FILE *f, struct thread *thread);
static int macho_parse_linkedit(FILE *f, struct linkedit_data *linkedit);


int macho_parse(FILE *f, union macho *macho) {
   /* read magic bytes */
   uint32_t magic;
   if (fread_exact(&magic, sizeof(magic), 1, f) < 0) {
      return -1;
   }

   switch (magic) {
   case FAT_MAGIC:
   case FAT_CIGAM:
      return macho_parse_fat(f, &magic, &macho->fat);

   case MH_MAGIC:
   case MH_CIGAM:
      return macho_parse_archive(f, &magic, &macho->archive);
      
   case MH_MAGIC_64:
   case MH_CIGAM_64:
      fprintf(stderr, "macho_parse: parsing 64-bit mach-o files not supported\n");
      return -1;

   default:
      fprintf(stderr, "macho_parse: file not recognized as Mach-O\n");
      return -1;
   }
}

int macho_parse_fat(FILE *f, const uint32_t *magic, struct fat *fat) {
   /* parse header */
   void *hdr_start;
   size_t hdr_len;
   if (magic) {
      fat->header.magic = *magic;
      hdr_start = AFTER(fat->header.magic);
      hdr_len = STRUCT_REM(fat->header, magic);
   } else {
      hdr_start = &fat->header;
      hdr_len = sizeof(fat->header);
   }

   if (fread_exact(hdr_start, hdr_len, 1, f) < 0) {
      return -1;
   }

   /* allocate archives */
   if ((fat->archives = calloc(fat->header.nfat_arch, sizeof(*fat->archives))) == NULL) {
      perror("malloc");
      return -1;
   }
   
   /* parse archives */
   for (uint32_t i = 0; i < fat->header.nfat_arch; ++i) {
      struct fat_archive *archive = &fat->archives[i];
      
      /* parse fat archive header */
      if (fread_exact(&archive->header, sizeof(archive->header), 1, f) < 0) {
         return -1;
      }

      /* parse data */
      if ((archive->data = fmread(1, archive->header.size, f)) == NULL) {
         return -1;
      }
   }

   return 0;
}


int macho_parse_archive(FILE *f, const uint32_t *magicp, union archive *archive) {
   /* get magic bytes */
   uint32_t magic;
   if (magic) {
      magic = *magicp;
   } else {
      if (fread_exact(&magic, sizeof(magic), 1, f) < 0) {
         return -1;
      }
   }

   /* switch on bitness */
   switch (magic) {
   case MH_MAGIC:
   case MH_CIGAM:
      return macho_parse_archive_32(f, &magic, &archive->archive_32);
      
   case MH_MAGIC_64:
   case MH_CIGAM_64:
      fprintf(stderr, "macho_parse_archive: parsing 64-bit Mach-O unsupported\n");
      return -1;

   default:
      fprintf(stderr, "macho_parse_archive: unexpected magic bytes for archive\n");
      return -1;
   }
}

int macho_parse_archive_32(FILE *f, const uint32_t *magic, struct archive_32 *archive) {
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
      union load_command_32 *command = &archive->commands[i];
      if (macho_parse_load_command_32(f, command) < 0) {
         return -1;
      }
   }

   return 0;
}

int macho_parse_load_command_32(FILE *f, union load_command_32 *command) {
   off_t offset;

   /* save offset */
   if ((offset = ftell(f)) < 0) {
      perror("ftell");
   }
   
   /* parse shared load command preamble */
   if (fread_exact(&command->load, sizeof(command->load), 1, f) < 0) {
      return -1;
   }

   switch (command->load.cmd) {
   case LC_SEGMENT:
      if (macho_parse_segment_32(f, &command->segment) < 0) {
         return -1;
      }
      break;

   case LC_SYMTAB:
      if (macho_parse_symtab_32(f, &command->symtab) < 0) {
         return -1;
      }
      break;

   case LC_DYSYMTAB:
      if (macho_parse_dysymtab_32(f, &command->dysymtab) < 0) {
         return -1;
      }
      break;

   case LC_LOAD_DYLINKER:
      if (macho_parse_dylinker(f, &command->dylinker) < 0) {
         return -1;
      }
      break;

   case LC_UUID:
      if (macho_parse_uuid(f, &command->uuid) < 0) {
         return -1;
      }
      break;

   case LC_UNIXTHREAD:
      if (macho_parse_thread(f, &command->thread) < 0) {
         return -1;
      }
      break;

   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      if (macho_parse_linkedit(f, &command->linkedit) < 0) {
         return -1;
      }
      break;
      
   default:
      fprintf(stderr, "macho_parse_load_command_32: unrecognized load command type 0x%x\n", command->load.cmd);
      return -1;
   }

   /* restore offset */
   if (fseek(f, offset + command->load.cmdsize, SEEK_SET) < 0) {
      perror("fseek");
      return -1;
   }

   return 0;
}

/* NOTE: Load command prefix already parsed. 
 * NOTE: Leaves file offset in indeterminate state. */
static int macho_parse_segment_32(FILE *f, struct segment_32 *segment) {
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
         struct section_wrapper_32 *sectwr = &segment->sections[i];

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
         struct section_wrapper_32 *sectwr = &segment->sections[i];
         uint32_t data_size = sectwr->section.size;
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

   // DEBUG
#if 1
   printf("LC_SEGMENT={\n\t.segname='%s',\n\t.fileoff=0x%x,\n\t.filesize=0x%x\n\n",
          segment->command.segname, segment->command.fileoff, segment->command.filesize);
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      printf("\tstruct section={.sectname='%s',.offset=0x%x,.size=0x%x}\n",
             segment->sections[i].section.sectname, segment->sections[i].section.offset,
             segment->sections[i].section.size);
      for (size_t j = 0; j < segment->sections[i].section.size; ++j) {
         printf(" %02x", ((uint8_t *)segment->sections[i].data)[j]);
      }
      printf("\n");
   }
#endif

   return 0;
}

static int macho_parse_symtab_32(FILE *f, struct symtab_32 *symtab) {
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
      struct nlist *entry = &symtab->entries[i];
      printf("\t{name='%s',value=0x%x,desc=0x%x,type=0x%x,sect=0x%x}\n",
             &symtab->strtab[entry->n_un.n_strx],
             entry->n_value,
             entry->n_desc,
             entry->n_type,
             entry->n_sect
             );
   }
   printf("}\n");
   
   return 0;
}

static int macho_parse_dysymtab_32(FILE *f, struct dysymtab_32 *dysymtab) {
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

static int macho_parse_dylinker(FILE *f, struct dylinker *dylinker) {
   /* parse dylinker command */
   /* NOTE: This might be buggy. */
   if (fread_exact(AFTER(dylinker->command.cmdsize),
                   STRUCT_REM(dylinker->command, cmdsize), 1, f) < 0) {
      return -1;
   }

   /* allocate space for name */
   size_t name_len = dylinker->command.cmdsize - sizeof(dylinker->command);
   if ((dylinker->name = calloc(name_len + 1, 1))
       == NULL) {
      return -1;
   }

   /* parse name */
   if (fread_exact(dylinker->name, 1, name_len, f) < 0) {
      return -1;
   }

   return 0;
}

static int macho_parse_uuid(FILE *f, struct uuid_command *uuid) {
   return fread_exact(AFTER(uuid->cmdsize), STRUCT_REM(*uuid, cmdsize), 1, f);
}

static int macho_parse_thread(FILE *f, struct thread *thread) {
   size_t count = 0;

   /* get thread structure data */
   uint32_t *data;
   size_t nitems = (thread->command.cmdsize - sizeof(struct load_command)) / sizeof(uint32_t);
   if ((data = fmread(sizeof(uint32_t), nitems, f)) == NULL) { return -1; }
   
   /* count the number of data structures */
   for (size_t i = 0; i < nitems; ) {
      ++count;

      /* skip flavor */
      ++i;

      /* skip state */
      i += data[i] + 1;
   }

   /* allocate array */
   if ((thread->entries = calloc(count, sizeof(thread->entries[0]))) == NULL) {
      perror("calloc");
      return -1;
   }

   thread->nentries = count;

   /* parse entries */
   uint32_t *data_it = data;
   for (size_t i = 0; i < count; ++i) {
      struct thread_entry *entry = &thread->entries[i];
      
      entry->flavor = *data_it++;
      entry->count = *data_it++;

      if ((entry->state = calloc(entry->count, sizeof(uint32_t))) == NULL) {
         perror("calloc");
         return -1;
      }
      memcpy(entry->state, data_it, sizeof(uint32_t) * entry->count);
      data_it += entry->count;
   }

   // DEBUG
   printf("thread entries = %d\n", thread->nentries);
   for (uint32_t i = 0; i < thread->nentries; ++i) {
      printf(".flavor=%d, .count=%d\n", thread->entries[i].flavor, thread->entries[i].count);
   }
   
   return 0;
}

static int macho_parse_linkedit(FILE *f, struct linkedit_data *linkedit) {
   if (fread_exact(AFTER(linkedit->command.cmdsize),
                   STRUCT_REM(linkedit->command, cmdsize), 1, f) < 0) {
      return -1;
   }

   /* parse data */
   if ((linkedit->data = malloc(linkedit->command.datasize)) == NULL) {
      perror("malloc");
      return -1;
   }
   if (fseek(f, linkedit->command.dataoff, SEEK_SET) < 0) {
      perror("fseek");
      return -1;
   }
   if (fread_exact(linkedit->data, 1, linkedit->command.datasize, f) < 0) {
      return -1;
   }
   
   // DEBUG
   printf("parsed linkedit blob with size 0x%x: ", linkedit->command.datasize);
   for (uint32_t i = 0; i < linkedit->command.datasize / 4; ++i) {
      printf(" %x", ((uint32_t *) linkedit->data)[i]);
   }
   printf("\n");

   return 0; 
}


