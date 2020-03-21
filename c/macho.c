#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>

#include "macho.h"
#include "util.h"

static int macho_parse_segment_32(FILE *f, struct segment_32 *segment);
static int macho_parse_symtab_32(FILE *f, struct symtab_32 *symtab);
static int macho_parse_dysymtab_32(FILE *f, struct dysymtab_32 *dysymtab);
static int macho_parse_dylinker(FILE *f, struct dylinker *dylinker);
static int macho_parse_uuid(FILE *f, struct uuid_command *uuid);
static int macho_parse_thread(FILE *f, struct thread_command *thread);
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
      // hdr_start = &fat->header.magic + sizeof(fat->header.magic);
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

   /* check if no data */
   if (segment->command.filesize == 0) {
      segment->data = NULL;
      return 0;
   }

   /* seek to data */
   if (fseek(f, segment->command.fileoff, SEEK_SET) < 0) {
      fprintf(stderr, "fseek: %u: %s\n", segment->command.fileoff, strerror(errno));
      return -1;
   }

   /* parse data */
   // TODO
   if ((segment->data = fmread(segment->command.filesize, 1, f)) == NULL) {
      return -1;
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
   // TODO

   /* parse string table */
   // TODO

   return 0;
}

static int macho_parse_dysymtab_32(FILE *f, struct dysymtab_32 *dysymtab) {
   /* parse dysymtab command */
   if (fread_exact(AFTER(dysymtab->command.cmdsize),
                   STRUCT_REM(dysymtab->command, cmdsize), 1, f) < 0) {
      return -1;
   }

   /* parse data */
   // TODO

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

static int macho_parse_thread(FILE *f, struct thread_command *thread) {
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
   
   return 0; 
}
