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
      fprintf(stderr, "macho_emit_archive: emitting 32-bit Mach-O archive not supported\n");
      return -1;
   }
}

int macho_emit_archive_32(FILE *f, const struct archive_32 *archive) {
   /* emit header */
   if (fwrite_exact(&archive->header, sizeof(archive->header), 1, f) < 0) {
      return -1;
   }

   /* emit commands */
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      if (macho_emit_load_command_32(f, &archive->commands[i]) < 0) {
         return -1;
      }
   }

   return 0;
}

int macho_emit_load_command_32(FILE *f, const union load_command_32 *command) {
   off_t start_pos;

   /* get command start position */
   if ((start_pos = ftell(f)) < 0) {
      perror("ftell");
      return -1;
   }

   switch (command->load.cmd) {
   case LC_SEGMENT:
      if (macho_emit_segment_32(f, &command->segment) < 0) { return -1; }
      break;
      
   case LC_SYMTAB:
      if (macho_emit_symtab_32(f, &command->symtab) < 0) { return -1; }
      break;
      
   case LC_DYSYMTAB:
      if (fwrite_exact(&command->dysymtab.command, sizeof(command->dysymtab.command), 1, f) < 0)
         { return -1; }
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
      
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      if (macho_emit_linkedit_data(f, &command->linkedit) < 0) { return -1; }
      break;
   }

   /* seek next cmdsize bytes */
   if (fseek(f, start_pos + command->load.cmdsize, SEEK_SET) < 0) {
      perror("fseek");
      return -1;
   }
   
   return 0;
}

static int macho_emit_segment_32(FILE *f, const struct segment_32 *segment) {
   /* emit segment_command struct */
   if (fwrite_exact(&segment->command, sizeof(segment->command), 1, f) < 0) {
      return -1;
   }

   /* emit section headers */
   uint32_t nsects = segment->command.nsects;
   for (uint32_t i = 0; i < nsects; ++i) {
      const struct section_wrapper_32 *sectwr = &segment->sections[i];
      if (fwrite_exact(&sectwr->section, sizeof(sectwr->section), 1, f) < 0) { return -1; }
   }

   /* emit section data */
   for (uint32_t i = 0; i < nsects; ++i) {
      const struct section_wrapper_32 *sectwr = &segment->sections[i];
      if (fseek(f, sectwr->section.offset, SEEK_SET) < 0) {
         perror("fseek");
         return -1;
      }
      if (fwrite_exact(sectwr->data, 1, sectwr->section.size, f) < 0) { return -1; }
   }

   return 0;
}

static int macho_emit_symtab_32(FILE *f, const struct symtab_32 *symtab) {
   /* emit symtab command */
   if (fwrite_exact(&symtab->command, sizeof(symtab->command), 1, f) < 0) { return -1; }

   /* emit symbol table entries */
   if (fwrite_at(symtab->entries, sizeof(struct nlist), symtab->command.nsyms, f,
                 symtab->command.symoff) < 0) { return -1; }

   /* emit string table */
   if (fwrite_at(symtab->strtab, 1, symtab->command.strsize, f, symtab->command.stroff) < 0) {
      return -1;
   }

   return 0;
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
#if 0
   if (fwrite_at(dylinker->name, sizeof(char), strlen(dylinker->name), f,
                 dylinker->command.name.offset + name_pos) < 0) { return -1; }
#else
   if (fwrite_exact(dylinker->name, sizeof(char), strlen(dylinker->name), f) < 0) { return -1; }
#endif

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
