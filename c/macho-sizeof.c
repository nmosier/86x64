#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "macho.h"
#include "macho-sizeof.h"

#define MACHO_BITS 32
#include "macho-sizeof-t.c"
#define MACHO_BITS 64
#include "macho-sizeof-t.c"


uint32_t macho_sizeof_thread(struct thread *thread) {
   uint32_t size = sizeof(thread->command);

   struct thread_list *it = thread->entries;
   while (it) {
      size += it->entry.header.count * sizeof(uint32_t) + sizeof(it->entry.header);
      it = it->next;
   }
   
   return thread->command.cmdsize = size;
}

