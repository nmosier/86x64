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

static int macho_parse_dylinker(FILE *f, struct dylinker *dylinker);
static int macho_parse_uuid(FILE *f, struct uuid_command *uuid);
static int macho_parse_thread(FILE *f, struct thread *thread);
static int macho_parse_linkedit(FILE *f, struct linkedit_data *linkedit);
static int macho_parse_dyld_info(FILE *f, struct dyld_info *dyld_info);
static int macho_parse_dylib(FILE *f, struct dylib_wrapper *dylib);
static int macho_parse_build_version(FILE *f, struct build_version *build);

static int macho_parse_segment_64(FILE *f, struct segment_64 *segment);

#define MACHO_BITS 32
#include "macho-parse-t.c"
#define MACHO_BITS 64
#include "macho-parse-t.c"

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
   case MH_MAGIC_64:
   case MH_CIGAM_64:
      return macho_parse_archive(f, &magic, &macho->archive);
      
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
      return macho_parse_archive_64(f, &magic, &archive->archive_64);

   default:
      fprintf(stderr, "macho_parse_archive: unexpected magic bytes for archive\n");
      return -1;
   }
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

static int macho_parse_build_version(FILE *f, struct build_version *build) {
   /* parse command */
   if (fread_exact(AFTER(build->command.cmdsize), STRUCT_REM(build->command, cmdsize), 1, f) < 0) {
      return -1;
   }

   /* parse tools */
   uint32_t ntools = build->command.ntools;
   if (ntools == 0) {
      build->tools = NULL;
   } else {
      if ((build->tools = calloc(ntools, sizeof(build->tools[0]))) == NULL) {
         perror("calloc");
         return -1;
      }
      if (fread_exact(build->tools, sizeof(build->tools[0]), ntools, f) < 0) { return -1; }
   }

   return 0;
}
