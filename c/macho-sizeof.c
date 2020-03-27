#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "macho.h"
#include "macho-sizeof.h"

#define MACHO_BITS 32
#include "macho-sizeof-t.c"
#define MACHO_BITS 64
#include "macho-sizeof-t.c"

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
