#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "macho.h"
#include "macho-sizeof.h"
#include "macho-util.h"

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
   case LC_DYLD_INFO_ONLY:
   case LC_VERSION_MIN_MACOSX:
   case LC_SOURCE_VERSION:
   case LC_MAIN:
      return command->load.cmdsize;
      
   case LC_LOAD_DYLINKER:
      return macho_sizeof_dylinker(&command->dylinker);
      
   case LC_UNIXTHREAD:
      return macho_sizeof_thread(&command->thread);

   case LC_LOAD_DYLIB:
      return macho_sizeof_dylib(&command->dylib);

   default:
      fprintf(stderr, "macho_sizeof_load_command_32: unrecognized load command 0x%x\n",
              command->load.cmd);
      abort();
   }
}

uint32_t macho_sizeof_segment_32(struct segment_32 *segment) {
   return segment->command.cmdsize =
      sizeof(segment->command) + segment->command.nsects *
      MACHO_SIZEOF(segment->sections[0].section, MACHO_32_PLACEHOLDER);
}

uint32_t macho_sizeof_dylinker(struct dylinker *dylinker) {
   uint32_t namelen = strlen(dylinker->name);
   return dylinker->command.cmdsize = sizeof(dylinker->command) + ALIGN_UP(namelen, sizeof(uint32_t));
}

uint32_t macho_sizeof_thread(struct thread *thread) {
   uint32_t size = sizeof(thread->command);

   struct thread_list *it = thread->entries;
   while (it) {
      size += it->entry.header.count * sizeof(uint32_t) + sizeof(it->entry.header);
      it = it->next;
   }
   
   return thread->command.cmdsize = size;
}

uint32_t macho_sizeof_dylib(struct dylib_wrapper *dylib) {
   return dylib->command.cmdsize = sizeof(dylib->command) +
      ALIGN_UP(strlen(dylib->name), sizeof(uint32_t));
}
