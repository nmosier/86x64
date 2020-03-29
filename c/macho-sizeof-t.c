#if MACHO_BITS == 32

# define SYMTAB symtab_32
# define DYSYMTAB dysymtab_32
# define SEGMENT segment_32
# define SECTION_WRAPPER section_wrapper_32
# define ARCHIVE archive_32
# define LOAD_COMMAND load_command_32
# define SECTION section
# define NLIST nlist

# define macho_sizeof_load_command macho_sizeof_load_command_32
# define macho_sizeof_segment macho_sizeof_segment_32
# define macho_sizeof_dylib macho_sizeof_dylib_32
# define macho_sizeof_dylinker macho_sizeof_dylinker_32
# define macho_vmsizeof_segment macho_vmsizeof_segment_32

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

# define macho_sizeof_load_command macho_sizeof_load_command_64
# define macho_sizeof_segment macho_sizeof_segment_64
# define macho_sizeof_dylib macho_sizeof_dylib_64
# define macho_sizeof_dylinker macho_sizeof_dylinker_64
# define macho_vmsizeof_segment macho_vmsizeof_segment_64

# define MACHO_SIZE_T uint64_t
# define MACHO_ADDR_T uint64_t

#endif

uint32_t macho_sizeof_dylib(struct dylib_wrapper *dylib);
uint32_t macho_sizeof_dylinker(struct dylinker *dylinker);

uint32_t macho_sizeof_load_command(union LOAD_COMMAND *command) {
   switch (command->load.cmd) {
#if MACHO_BITS == 32
   case LC_SEGMENT:
#else
   case LC_SEGMENT_64:
#endif
      return macho_sizeof_segment(&command->segment);

      /* These command sizes never change. */
   case LC_SYMTAB:
   case LC_DYSYMTAB:
   case LC_UUID:
   case LC_CODE_SIGNATURE:
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
   case LC_DYLD_INFO_ONLY:
   case LC_VERSION_MIN_MACOSX:
   case LC_SOURCE_VERSION:
   case LC_MAIN:
   case LC_BUILD_VERSION:
      return command->load.cmdsize;
      
   case LC_LOAD_DYLINKER:
      return macho_sizeof_dylinker(&command->dylinker);
      
   case LC_UNIXTHREAD:
      return macho_sizeof_thread(&command->thread);

   case LC_ID_DYLIB:
   case LC_LOAD_DYLIB:
      return macho_sizeof_dylib(&command->dylib);

   default:
      fprintf(stderr, "macho_sizeof_load_command: unrecognized load command 0x%x\n",
              command->load.cmd);
      abort();
   }
}

uint32_t macho_sizeof_segment(struct SEGMENT *segment) {
   return segment->command.cmdsize =
      sizeof(segment->command) + segment->command.nsects * sizeof(segment->sections[0].section);
}

uint32_t macho_sizeof_dylib(struct dylib_wrapper *dylib) {
   return dylib->command.cmdsize =
      ALIGN_UP(sizeof(dylib->command) + strlen(dylib->name), sizeof(MACHO_SIZE_T));
}

uint32_t macho_sizeof_dylinker(struct dylinker *dylinker) {
   return dylinker->command.cmdsize
      = ALIGN_UP(sizeof(dylinker->command) + strlen(dylinker->name), sizeof(MACHO_SIZE_T));
}

MACHO_SIZE_T macho_vmsizeof_segment(const struct SEGMENT *segment) {
   uint32_t nsects = segment->command.nsects;

   MACHO_SIZE_T size = 0;
   for (uint32_t i = 0; i < nsects; ++i) {
      struct SECTION *section = &segment->sections[i].section;
      size = ALIGN_UP(size, 1 << section->align) + section->size;
   }

   return ALIGN_UP(size, PAGESIZE);
}



#undef SYMTAB
#undef DYSYMTAB
#undef SEGMENT
#undef SECTION_WRAPPER
#undef ARCHIVE
#undef LOAD_COMMAND
#undef SECTION
#undef NLIST

#undef macho_sizeof_load_command
#undef macho_sizeof_segment
#undef macho_sizeof_dylib
#undef macho_sizeof_dylinker
#undef macho_vmsizeof_segment

#undef MACHO_SIZE_T
#undef MACHO_ADDR_T

#undef MACHO_BITS
