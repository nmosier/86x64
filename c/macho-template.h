#ifndef MACHO_BITS

# if MACHO_BITS == 32
  
#  define SYMTAB symtab_32
#  define DYSYMTAB dysymtab_32
#  define SEGMENT segment_32
#  define SECTION_WRAPPER section_wrapper_32
#  define ARCHIVE archive_32
#  define LOAD_COMMAND load_command_32
#  define SECTION section
#  define NLIST nlist
  
#  define MACHO_SIZE_T uint32_t
#  define MACHO_ADDR_T uint32_t

#  define decl(fn) fn##_32
  
# else
  
#  define SYMTAB symtab_64
#  define DYSYMTAB dysymtab_64
#  define SEGMENT segment_64
#  define SECTION_WRAPPER section_wrapper_64
#  define ARCHIVE archive_64
#  define LOAD_COMMAND load_command_64
#  define SECTION section_64
#  define NLIST nlist_64
  
#  define MACHO_SIZE_T uint64_t
#  define MACHO_ADDR_T uint64_t

#  define decl(fn) fn##_64
  
# endif

#else
  
# undef SYMTAB
# undef DYSYMTAB
# undef SEGMENT
# undef SECTION_WRAPPER
# undef ARCHIVE
# undef LOAD_COMMAND
# undef SECTION
# undef NLIST

# undef MACHO_SIZE_T
# undef MACHO_ADDR_T

# undef decl
  
# undef MACHO_BITS

#endif
