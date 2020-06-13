#include "macho.h"

#include "macho-util.h"
#include "macho-insert.h"

#define MACHO_BITS 32
#include "macho-insert-t.c"
#define MACHO_BITS 64
#include "macho-insert-t.c"

int macho_insert_instruction(const void *instbuf, size_t instlen, uint64_t vmaddr,
                             union archive *archive) {
   switch (macho_bits(archive)) {
   case MACHO_32:
      return macho_insert_instruction_32(instbuf, instlen, vmaddr, &archive->archive_32);
   case MACHO_64:
      return macho_insert_instruction_64(instbuf, instlen, vmaddr, &archive->archive_64);
   }
}
