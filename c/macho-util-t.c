#if MACHO_BITS == 32

# define ARCHIVE         archive_32
# define LOAD_COMMAND    load_command_32
# define SEGMENT         segment_32
# define SECTION         section
# define SECTION_WRAPPER section_wrapper_32

# define macho_find_segment      macho_find_segment_32
# define macho_is_linkedit       macho_is_linkedit_32
# define macho_find_section      macho_find_section_32
# define macho_find_load_command macho_find_load_command_32
# define macho_linkedit          macho_linkedit_32
# define macho_index_segment     macho_index_segment_32
# define macho_vmaddr_to_section macho_vmaddr_to_section_32
# define macho_vmaddr_to_ptr     macho_vmaddr_to_ptr_32

# define MACHO_SIZE_T uint32_t
# define MACHO_ADDR_T uint32_t

#else

# define ARCHIVE      archive_64
# define LOAD_COMMAND load_command_64
# define SEGMENT      segment_64
# define SECTION         section_64
# define SECTION_WRAPPER section_wrapper_64

# define macho_find_segment      macho_find_segment_64
# define macho_is_linkedit       macho_is_linkedit_64
# define macho_find_section      macho_find_section_64
# define macho_find_load_command macho_find_load_command_64
# define macho_linkedit          macho_linkedit_64
# define macho_index_segment     macho_index_segment_64
# define macho_vmaddr_to_section macho_vmaddr_to_section_64
# define macho_vmaddr_to_ptr     macho_vmaddr_to_ptr_64

# define MACHO_SIZE_T uint64_t
# define MACHO_ADDR_T uint64_t

#endif

struct SEGMENT *macho_find_segment(const char *segname, struct ARCHIVE *archive) {
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      union LOAD_COMMAND *command = &archive->commands[i];
      if (command->load.cmd ==
#if MACHO_BITS == 32
          LC_SEGMENT
#else
          LC_SEGMENT_64
#endif
          ) {
         struct SEGMENT *segment = &command->segment;
         if (strncmp(segment->command.segname, segname, sizeof(segment->command.segname)) == 0) {
            return segment;
         }
      }
   }

   return NULL;
}

bool macho_is_linkedit(const union LOAD_COMMAND *command) {
   switch (command->load.cmd) {
   case LC_CODE_SIGNATURE:
   case LC_SEGMENT_SPLIT_INFO:
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
   case LC_DYLIB_CODE_SIGN_DRS:
   case LC_LINKER_OPTIMIZATION_HINT:
      return true;
                  
   default:
      return false;
   }
}


struct SECTION_WRAPPER *macho_find_section(const char *sectname, struct SEGMENT *segment)
{
   for (uint32_t i = 0; i < segment->command.nsects; ++i) {
      struct SECTION_WRAPPER *sectwr = &segment->sections[i];
      if (strncmp(sectwr->section.sectname, sectname, sizeof(sectwr->section.sectname)) == 0) {
         return sectwr;
      }
   }

   return NULL;
}

union LOAD_COMMAND *macho_find_load_command(uint32_t cmd, struct ARCHIVE *archive) {
   for (uint32_t i = 0; i < archive->header.ncmds; ++i) {
      union LOAD_COMMAND *command = &archive->commands[i];
      if (command->load.cmd == cmd) {
         return command;
      } 
   }

   return NULL;
}


struct linkedit_data *macho_linkedit(union LOAD_COMMAND *command) {
   return macho_is_linkedit(command) ? &command->linkedit : NULL;
}


struct SEGMENT *macho_index_segment(uint32_t index, struct ARCHIVE *archive) {
   uint32_t ncmds = archive->header.ncmds;

   uint32_t found = 0;
   for (uint32_t i = 0; i < ncmds; ++i) {
      union LOAD_COMMAND *command = &archive->commands[i];
      if (command->load.cmd ==
#if MACHO_BITS == 32
          LC_SEGMENT
#else
          LC_SEGMENT_64
#endif
          && found++ == index) {
         return &command->segment;
      }
   }

   return NULL;
}

struct SECTION_WRAPPER *macho_vmaddr_to_section(MACHO_ADDR_T addr, struct SEGMENT *segment) {
   const uint32_t nsects = segment->command.nsects;
   for (uint32_t i = 0; i < nsects; ++i) {
      struct SECTION_WRAPPER *sectwr = &segment->sections[i];
      struct SECTION *section = &sectwr->section;
      if (addr >= section->addr && addr < section->addr + section->size) {
         return sectwr;
      }
   }
   return NULL;
}

void *macho_vmaddr_to_ptr(MACHO_ADDR_T addr, struct SEGMENT *segment) {
   struct SECTION_WRAPPER *sectwr;

   /* find section */
   if ((sectwr = macho_vmaddr_to_section(addr, segment)) == NULL) {
      return NULL;
   }

   /* find offset within section */
   const MACHO_SIZE_T off = addr - sectwr->section.addr;

   /* return pointer */
   return (uint8_t *) sectwr->data + off;
}




#undef ARCHIVE
#undef LOAD_COMMAND
#undef SEGMENT
#undef SECTION
#undef SECTION_WRAPPER

#undef macho_find_segment
#undef macho_is_linkedit
#undef macho_find_section
#undef macho_find_load_command
#undef macho_linkedit
#undef macho_index_segment
#undef macho_vmaddr_to_section
#undef macho_vmaddr_to_ptr

#undef MACHO_ADDR_T
#undef MACHO_SIZE_T

#undef MACHO_BITS
