#include <string.h>
#include <mach-o/nlist.h>

#include "macho-emit.h"
#include "macho-util.h"
#include "util.h"

/* NOTE: Leaves f file position in unspecified state. */
static int macho_emit_segment_32(FILE *f, const struct segment_32 *segment);
static int macho_emit_symtab_32(FILE *f, const struct symtab_32 *symtab);
static int macho_emit_dylinker(FILE *f, const struct dylinker *dylinker);
static int macho_emit_linkedit_data(FILE *f, const struct linkedit_data *linkedit);
static int macho_emit_thread(FILE *f, const struct thread *thread);
static int macho_emit_dyld_info(FILE *f, const struct dyld_info *dyld_info);
static int macho_emit_dylib(FILE *f, const struct dylib_wrapper *dylib);
static int macho_emit_dysymtab_32(FILE *f, const struct dysymtab *dysymtab);
static int macho_emit_build_version(FILE *f, const struct build_version *build);

#define MACHO_BITS 32
#include "macho-emit-t.c"
#define MACHO_BITS 64
#include "macho-emit-t.c"

int macho_emit(FILE *f, const union macho *macho) {
   switch (macho_kind(macho)) {
   case MACHO_FAT:
      fprintf(stderr, "macho_emit: emitting Mach-O fat binaries not supported\n");
      return -1;
      
   case MACHO_ARCHIVE:
      return macho_emit_archive(f, &macho->archive);
   }
}

int macho_emit_archive(FILE *f, const union archive *archive) {
   switch (macho_bits(archive)) {
   case MACHO_32:
      return macho_emit_archive_32(f, &archive->archive_32);
      
   case MACHO_64:
      return macho_emit_archive_64(f, &archive->archive_64);
   }
}



static int macho_emit_dylinker(FILE *f, const struct dylinker *dylinker) {
   /* emit dylinker command */
   if (fwrite_exact(&dylinker->command, sizeof(dylinker->command), 1, f) < 0) { return -1; }

   /* emit dylinker name */
   off_t name_pos;
   if ((name_pos = ftell(f)) < 0) {
      perror("ftell");
      return -1;
   }
   if (fwrite_exact(dylinker->name, sizeof(char), strlen(dylinker->name), f) < 0) { return -1; }

   return 0;
}

static int macho_emit_linkedit_data(FILE *f, const struct linkedit_data *linkedit) {
   /* emit linkedit_data command */
   if (fwrite_exact(&linkedit->command, sizeof(linkedit->command), 1, f) < 0) { return -1; }

   /* emit data */
   if (fwrite_at(linkedit->data, sizeof(uint8_t), linkedit->command.datasize, f,
                 linkedit->command.dataoff) < 0) { return -1; }

   return 0;
}

static int macho_emit_thread(FILE *f, const struct thread *thread) {
   /* emit thread command */
   if (fwrite_exact(&thread->command, sizeof(thread->command), 1, f) < 0) { return -1; }

   /* emit state structures */
   struct thread_list *thread_it = thread->entries;
   while (thread_it) {
      /* write thread state header */
      if (fwrite_exact(&thread_it->entry.header, sizeof(thread_it->entry.header), 1, f) < 0) {
         return -1;
      }

      /* write thread state body */
      switch (thread_it->entry.header.flavor) {
      case x86_THREAD_STATE32:
         if (fwrite_exact(&thread_it->entry.x86_thread.uts.ts32,
                          sizeof(thread_it->entry.x86_thread.uts.ts32), 1, f) < 0) { return -1; }
         break;
         
      default:
         fprintf(stderr, "macho_emit_thread: unsupported thread state flavor 0x%x\n",
                 thread_it->entry.header.flavor);
         return -1;
      }
      
      /* increment iterator */
      thread_it = thread_it->next;
   }

   return 0;
}

static int macho_emit_dyld_info(FILE *f, const struct dyld_info *dyld_info) {
   /* emit dyld_info command */
   if (fwrite_exact(&dyld_info->command, sizeof(dyld_info->command), 1, f) < 0) { return -1; }

   /* emit rebase data */
   if (fwrite_at(dyld_info->rebase_data, 1, dyld_info->command.rebase_size, f,
                 dyld_info->command.rebase_off) < 0) { return -1; }

   /* emit bind data */
   if (fwrite_at(dyld_info->bind_data, 1, dyld_info->command.bind_size, f,
                 dyld_info->command.bind_off) < 0) { return -1; }

   /* emit lazy bind data */
   if (fwrite_at(dyld_info->lazy_bind_data, 1, dyld_info->command.lazy_bind_size, f,
                 dyld_info->command.lazy_bind_off) < 0) { return -1; }

   /* emit export data */
   if (fwrite_at(dyld_info->export_data, 1, dyld_info->command.export_size, f,
                 dyld_info->command.export_off) < 0) { return -1; }

   return 0;
}

static int macho_emit_dylib(FILE *f, const struct dylib_wrapper *dylib) {
   /* emit dylib command */
   if (fwrite_exact(&dylib->command, sizeof(dylib->command), 1, f) < 0) { return -1; }

   /* emit name */
   if (fwrite_exact(dylib->name, sizeof(char), strlen(dylib->name), f) < 0) { return -1; }

   return 0;
}

static int macho_emit_build_version(FILE *f, const struct build_version *build) {
   /* emit command */
   if (fwrite_exact(&build->command, sizeof(build->command), 1, f) < 0) { return -1; }

   /* emit tools */
   if (fwrite_exact(build->tools, sizeof(build->tools[0]), build->command.ntools, f) < 0) {
      return -1;
   }

   return 0;
}
