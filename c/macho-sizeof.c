#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "macho-parse.h"
#include "macho-sizeof.h"

uint32_t macho_sizeof_load_command_32(union load_command_32 *command) {
   switch (command->load.cmd) {
   case LC_SEGMENT:
      return macho_sizeof_segment_32(&command->segment);

      /* These command sizes never change. */
   case LC_SYMTAB:
   case LC_DYSYMTAB:
   case LC_UUID:      
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      return command->load.cmdsize;
      
   case LC_LOAD_DYLINKER:
      return macho_sizeof_dylinker(&command->dylinker);
      
   case LC_UNIXTHREAD:
      return macho_sizeof_thread(&command->thread);

   default:
      fprintf(stderr, "macho_sizeof_load_command_32: unrecognized load command 0x%x\n",
              command->load.cmd);
      abort();
   }
}

uint32_t macho_sizeof_segment_32(struct segment_32 *segment) {
   return segment->command.cmdsize =
      sizeof(segment->command) + segment->command.nsects * sizeof(segment->sections[0].section);
}

uint32_t macho_sizeof_dylinker(struct dylinker *dylinker) {
   uint32_t namelen = strlen(dylinker->name);
   return dylinker->command.cmdsize = sizeof(dylinker->command) + ALIGN_UP(namelen, 4);
}

uint32_t macho_sizeof_thread(struct thread *thread) {
   uint32_t size = sizeof(thread->command);

   for (uint32_t i = 0; i < thread->nentries; ++i) {
      struct thread_entry *entry = &thread->entries[i];
      size += sizeof(entry->flavor) + sizeof(entry->count);
      size += sizeof(entry->state[0]) * entry->count;
   }

   return thread->command.cmdsize = size;
}
