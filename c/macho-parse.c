#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/thread_status.h>

#include "macho.h"
#include "macho-parse.h"
#include "macho-util.h"
#include "util.h"

static int macho_parse_segment(FILE *f, struct segment_32 *segment, enum macho_bits bits);
static int macho_parse_symtab_32(FILE *f, struct symtab_32 *symtab);
static int macho_parse_dysymtab(FILE *f, struct dysymtab *dysymtab, enum macho_bits bits);
static inline int macho_parse_dysymtab_32(FILE *f, struct dysymtab *dysymtab) {
   return macho_parse_dysymtab(f, dysymtab, MACHO_32);
}
static inline int macho_parse_dysymtab_64(FILE *f, struct dysymtab *dysymtab) {
   return macho_parse_dysymtab(f, dysymtab, MACHO_64);
}

static int macho_parse_dylinker(FILE *f, struct dylinker *dylinker);
static int macho_parse_uuid(FILE *f, struct uuid_command *uuid);
static int macho_parse_thread(FILE *f, struct thread *thread);
static int macho_parse_linkedit(FILE *f, struct linkedit_data *linkedit);
static int macho_parse_dyld_info(FILE *f, struct dyld_info *dyld_info);
static int macho_parse_dylib(FILE *f, struct dylib_wrapper *dylib);

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
      return -1;
   }
   
   /* parse shared load command preamble */
   if (fread_exact(&command->load, sizeof(command->load), 1, f) < 0) { return -1; }

   switch (command->load.cmd) {
   case LC_SEGMENT:
      if (macho_parse_segment(f, &command->segment, MACHO_32) < 0) { return -1; }
      break;

   case LC_SYMTAB:
      if (macho_parse_symtab_32(f, &command->symtab) < 0) { return -1; }
      break;

   case LC_DYSYMTAB:
      if (macho_parse_dysymtab_32(f, &command->dysymtab) < 0) { return -1; }
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

   case LC_LOAD_DYLIB:
      if (macho_parse_dylib(f, &command->dylib) < 0) { return -1; }
      break;
      
   default:
      fprintf(stderr, "macho_parse_load_command_32: unrecognized load command type 0x%x\n",
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

/* NOTE: Load command prefix already parsed. 
 * NOTE: Leaves file offset in indeterminate state. */
static int macho_parse_segment(FILE *f, struct segment_32 *segment, enum macho_bits bits) {
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
         struct section_wrapper *sectwr = &segment->sections[i];

         /* read section struct */
         if (fread_exact(MACHO_MEMBPTR(sectwr->section, bits),
                         MACHO_SIZEOF(sectwr->section, bits),
                         1, f) < 0) { return -1; }

         /* guard against unsupported features */
         if (MACHO_MEMBER_SUFFIX(sectwr->section, bits, .nreloc) != 0) {
            // if (sectwr->section.nreloc != 0) {
            fprintf(stderr, "macho_parse_segment_32: relocation entries not supported\n");
            return -1;
         }
      }

      /* do section_wrapper initialization work */
      for (uint32_t i = 0; i < nsects; ++i) {
         struct section_wrapper *sectwr = &segment->sections[i];
         macho_size_t data_size = MACHO_MEMBER_SUFFIX(sectwr->section, bits, .size);
         // uint32_t data_size = sectwr->section.size;
         if (data_size == 0) {
            sectwr->data = NULL;
         } else {
            if ((sectwr->data = fmread_at(1, data_size, f,
                                          MACHO_MEMBER_SUFFIX(sectwr->section, bits, .offset)))
                == NULL) { return -1; }
         }
      }
   }

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

static int macho_parse_dysymtab(FILE *f, struct dysymtab *dysymtab, enum macho_bits bits) {
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
              "macho_parse_dysymtab_32: parsing local relocation entries not supported\n");
      return -1;
   }

   /* parse indirect syms */
   size_t sizeof_indirectsym = MACHO_SIZEOF(*dysymtab->indirectsyms, bits);
   if ((dysymtab->indirectsyms_xx =
        fmread_at(sizeof_indirectsym, dysymtab->command.nindirectsyms, f,
                  dysymtab->command.indirectsymoff)) == NULL)
      { return -1; }

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
   /* parse command */
   /* (already done) */

   /* parse thread list */
   struct thread_list **thread_it = &thread->entries;
   *thread_it = NULL;
   size_t rem = (thread->command.cmdsize - sizeof(struct thread)) / sizeof(uint32_t);
   while (rem > 0) {
      /* allocate new node */
      if ((*thread_it = malloc(sizeof(struct thread_list))) == NULL) {
         perror("malloc");
         return -1;
      }

      union thread_entry *entry = &(*thread_it)->entry;

      /* read header */
      if (fread_exact(&entry->header, sizeof(entry->header), 1, f) < 0) { return -1; }

      switch (entry->header.flavor) {
      case x86_THREAD_STATE32:
         if (fread_exact(&entry->x86_thread.uts.ts32, sizeof(entry->x86_thread.uts.ts32), 1, f) < 0)
            { return -1; }
         break;
         
      default:
         fprintf(stderr, "macho_parse_thread: thread flavor 0x%x not supported\n",
                 entry->header.flavor);
         return -1;
      }
      

      /* increment iterator */
      thread_it = &(*thread_it)->next;
      *thread_it = NULL;

      /* decrement remaining uint32_t's */
      rem -= entry->header.count;
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

static int macho_parse_dyld_info(FILE *f, struct dyld_info *dyld_info) {
   /* read rest of struct */
   if (fread_exact(AFTER(dyld_info->command.cmdsize),
                   STRUCT_REM(dyld_info->command, cmdsize), 1, f) < 0) { return -1; }

   /* feature checks */
   if (dyld_info->command.weak_bind_size) {
      fprintf(stderr, "macho_parse_dyld_info: weak bindings not supported\n");
      return -1;
   }

   if ((dyld_info->rebase_data = fmread_at(1, dyld_info->command.rebase_size, f,
                                           dyld_info->command.rebase_off)) == NULL) { return -1; }
   if ((dyld_info->bind_data = fmread_at(1, dyld_info->command.bind_size, f,
                                         dyld_info->command.bind_off)) == NULL) { return -1; }
   if ((dyld_info->lazy_bind_data = fmread_at(1, dyld_info->command.lazy_bind_size, f,
                                              dyld_info->command.lazy_bind_off)) == NULL)
      { return -1; }
   if ((dyld_info->export_data = fmread_at(1, dyld_info->command.export_size, f,
                                           dyld_info->command.export_off)) == NULL) { return -1; }

   return 0;
}

static int macho_parse_dylib(FILE *f, struct dylib_wrapper *dylib) {
   if (fread_exact(AFTER(dylib->command.cmdsize), 1,
                   STRUCT_REM(dylib->command, cmdsize), f) < 0) { return -1; }

   /* read in string */
   if ((dylib->name = fmread(1, dylib->command.cmdsize - sizeof(dylib->command), f)) == NULL) {
      return -1;
   }

   return 0;
}
