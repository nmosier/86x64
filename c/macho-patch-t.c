#if MACHO_BITS == 32

# define SYMTAB symtab_32
# define DYSYMTAB dysymtab_32
# define SEGMENT segment_32
# define SECTION_WRAPPER section_wrapper_32
# define ARCHIVE archive_32
# define LOAD_COMMAND load_command_32
# define SECTION section
# define NLIST nlist

# define macho_patch_TEXT_address macho_patch_TEXT_address_32

# define MACHO_SIZE_T uint32_t
# define MACHO_ADDR_T uint32_t

#else

# define SYMTAB symtab_64
# define DYSYMTAB dysymtab_64
# define SEGMENT segment_64
# define SECTION_WRAPPER section_wrapper_64
# define ARCHIVE archive_64
# define LOAD_COMMAND load_command_64
# define SECTION section_64
# define NLIST nlist_64

# define macho_patch_TEXT_address macho_patch_TEXT_address_64

# define MACHO_SIZE_T uint64_t
# define MACHO_ADDR_T uint64_t

#endif

#include "macho.h"

MACHO_ADDR_T macho_patch_TEXT_address(MACHO_ADDR_T addr, const struct SEGMENT *segment) {
   const uint32_t nsects = segment->command.nsects;
   for (uint32_t i = 0; i < nsects; ++i) {
      struct SECTION_WRAPPER *sectwr = &segment->sections[i];
      const struct SECTION *section = &sectwr->section;
      MACHO_ADDR_T adjusted_addr = addr + sectwr->adiff;
      if (adjusted_addr >= section->addr &&
          adjusted_addr < section->addr + section->size) {
         return adjusted_addr;
      }
   }

   return MACHO_BAD_ADDR;
}




#undef SYMTAB
#undef DYSYMTAB
#undef SEGMENT
#undef SECTION_WRAPPER
#undef ARCHIVE
#undef LOAD_COMMAND
#undef SECTION
#undef NLIST

#undef macho_patch_TEXT_address

#undef MACHO_SIZE_T
#undef MACHO_ADDR_T

#undef MACHO_BITS
