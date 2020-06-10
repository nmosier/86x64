#include <stdlib.h>
#include <string.h>
#include <mach/machine.h>

#include "macho-transform.h"
#include "util.h"

static int macho_transform_command_32to64(const union load_command_32 *cmd32,
                                          union load_command_64 *cmd64);
static int macho_transform_segment_32to64(const struct segment_32 *seg32,
                                          struct segment_64 *seg64);
static int macho_transform_section_32to64(const struct section_wrapper_32 *wr32,
                                          struct section_wrapper_64 *wr64);
static int macho_transform_symtab_32to64(const struct symtab_32 *symtab32,
                                         struct symtab_64 *symtab64);
static int macho_transform_nlist_32to64(const struct nlist *ent32, struct nlist_64 *ent64);
static int macho_transform_dysymtab_32to64(const struct dysymtab *dy32,
                                           struct dysymtab *dy64);
static int macho_transform_dyld_info_32to64(const struct dyld_info *dl32, struct dyld_info *dl64);
static int macho_transform_linkedit_32to64(const struct linkedit_data *linkedit32,
                                           struct linkedit_data *linkedit64);
static int macho_transform_rebase_info_32to64(const void *rebase_data_32, uint32_t rebase_size_32,
                                              void **rebase_data_64, uint32_t *rebase_size_64);
static int macho_transform_bind_info_32to64(const void *bind_data_32, uint32_t bind_size_32,
                                            void **bind_data_64, uint32_t *bind_size_64);

int macho_transform_archive_32to64(const struct archive_32 *archive32,
                                   struct archive_64 *archive64) {
   const struct mach_header *hdr32 = &archive32->header;
   struct mach_header_64 *hdr64 = &archive64->header;

   /* transform header */
   hdr64->magic = MH_MAGIC_64;
   hdr64->cputype = CPU_TYPE_X86_64;
   hdr64->cpusubtype = CPU_SUBTYPE_X86_64_ALL;
   hdr64->filetype = hdr32->filetype;
   hdr64->ncmds = hdr32->ncmds;
   hdr64->sizeofcmds = hdr32->sizeofcmds;
   hdr64->flags = hdr32->flags;

   /* flags 64-bit commands */
   if ((archive64->commands = calloc(hdr64->ncmds, sizeof(archive64->commands[0]))) == NULL) {
      perror("calloc");
      return -1;
   }

   /* transform commands */
   for (uint32_t i = 0; i < hdr32->ncmds; ++i) {
      if (macho_transform_command_32to64(&archive32->commands[i], &archive64->commands[i]) < 0) {
         return -1;
      }
   }

   return 0;
}

static int macho_transform_command_32to64(const union load_command_32 *cmd32,
                                          union load_command_64 *cmd64) {
   switch (cmd32->load.cmd) {
   case LC_SEGMENT:
      if (macho_transform_segment_32to64(&cmd32->segment, &cmd64->segment) < 0) { return -1; }
      break;
      
   case LC_SYMTAB:
      if (macho_transform_symtab_32to64(&cmd32->symtab, &cmd64->symtab) < 0) { return -1; }
      break;
      
   case LC_DYSYMTAB:
      if (macho_transform_dysymtab_32to64(&cmd32->dysymtab, &cmd64->dysymtab) < 0) { return -1; }
      break;

   case LC_LOAD_DYLINKER:
      cmd64->dylinker.command = cmd32->dylinker.command;
      cmd64->dylinker.name = strdup(cmd32->dylinker.name);
      break;

   case LC_UUID:
      cmd64->uuid = cmd32->uuid;
      break;

   case LC_UNIXTHREAD:
      fprintf(stderr, "transforming LC_UNIXTHREAD not supported\n");
      return -1;
      
   case LC_CODE_SIGNATURE:
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:
      if (macho_transform_linkedit_32to64(&cmd32->linkedit, &cmd64->linkedit) < 0) { return -1; }
      break;

   case LC_DYLD_INFO:
   case LC_DYLD_INFO_ONLY:
      if (macho_transform_dyld_info_32to64(&cmd32->dyld_info, &cmd64->dyld_info) < 0) { return -1; }
      break;

   case LC_VERSION_MIN_MACOSX:
      cmd64->version_min = cmd32->version_min;
      break;

   case LC_SOURCE_VERSION:
      cmd64->source_version = cmd32->source_version;
      break;

   case LC_MAIN:
      cmd64->entry_point = cmd32->entry_point;
      break;

   case LC_ID_DYLIB:
   case LC_LOAD_DYLIB:
      cmd64->dylib = cmd32->dylib;
      break;

   case LC_BUILD_VERSION:
      cmd64->build_version = cmd32->build_version;
      break;
      
   default:
      fprintf(stderr, "macho_transform_command_32to64: unrecognized load command type 0x%x\n",
              cmd32->load.cmd);
      return -1;
   }

   return 0;
}

static int macho_transform_segment_32to64(const struct segment_32 *seg32, struct segment_64 *seg64)
{
   const struct segment_command *cmd32 = &seg32->command;
   struct segment_command_64 *cmd64 = &seg64->command;
   cmd64->cmd      = LC_SEGMENT_64;
   cmd64->cmdsize  =
      ALIGN_UP(sizeof(struct segment_command_64) + cmd32->nsects * sizeof(struct section_64), 8);
   memcpy(cmd64->segname, cmd32->segname, sizeof(cmd64->segname));
   cmd64->vmaddr   = cmd32->vmaddr;
   cmd64->vmsize   = cmd32->vmsize;
   cmd64->fileoff  = cmd32->fileoff;
   cmd64->filesize = cmd32->filesize;
   cmd64->maxprot  = cmd32->maxprot;
   cmd64->initprot = cmd32->initprot;
   cmd64->nsects   = cmd32->nsects;
   cmd64->flags    = cmd32->flags;

   /* allocate sections */
   if ((seg64->sections = calloc(cmd32->nsects, sizeof(seg64->sections[0]))) == NULL) {
      perror("calloc");
      return -1;
   }

   for (uint32_t i = 0; i < cmd32->nsects; ++i) {
      if (macho_transform_section_32to64(&seg32->sections[i], &seg64->sections[i]) < 0) {
         return -1;
      }
   }

   return 0;
}

static int macho_transform_section_32to64(const struct section_wrapper_32 *wr32,
                                          struct section_wrapper_64 *wr64) {
   const struct section *sect32 = &wr32->section;
   struct section_64 *sect64 = &wr64->section;
   memcpy(sect64->sectname, sect32->sectname, sizeof(sect64->sectname));
   memcpy(sect64->segname,  sect32->segname,  sizeof(sect64->segname));
   sect64->addr      = sect32->addr;
   sect64->size      = sect32->size;
   sect64->offset    = sect32->offset;
   sect64->align     = sect32->align;
   sect64->reloff    = sect32->reloff;
   sect64->nreloc    = sect32->nreloc;
   sect64->flags     = sect32->flags;
   sect64->reserved1 = sect32->reserved1;
   sect64->reserved2 = sect32->reserved2;
   sect64->reserved3 = 0;

   wr64->data  = wr32->data;
   wr64->adiff = 0;
   wr64->odiff = 0;

   return 0;
}

static int macho_transform_symtab_32to64(const struct symtab_32 *symtab32,
                                         struct symtab_64 *symtab64) {
   const struct symtab_command *cmd32 = &symtab32->command;
   struct symtab_command *cmd64 = &symtab64->command;
   
   *cmd64 = *cmd32;

   if ((symtab64->entries = calloc(cmd64->nsyms, sizeof(symtab64->entries[0]))) == NULL) {
      perror("calloc");
      return -1;
   }
   
   for (uint32_t i = 0; i < cmd64->nsyms; ++i) {
      if (macho_transform_nlist_32to64(&symtab32->entries[i], &symtab64->entries[i]) < 0) {
         return -1;
      }
   }

   if ((symtab64->strtab = malloc(cmd64->strsize)) == NULL) {
      perror("malloc");
      return -1;
   }
   
   memcpy(symtab64->strtab, symtab32->strtab, cmd64->strsize);
   
   return 0;
}

static int macho_transform_nlist_32to64(const struct nlist *ent32, struct nlist_64 *ent64) {
   ent64->n_un.n_strx = ent32->n_un.n_strx;
   ent64->n_type = ent32->n_type;
   ent64->n_sect = ent32->n_sect;
   ent64->n_desc = ent32->n_desc;
   ent64->n_value = ent32->n_value; // TODO: Can we really extend value from 32 to 64 bits?
   return 0;
}

static int macho_transform_dysymtab_32to64(const struct dysymtab *dy32, struct dysymtab *dy64) {
   const struct dysymtab_command *cmd32 = &dy32->command;
   struct dysymtab_command *cmd64 = &dy64->command;
   *cmd64 = *cmd32;

   if ((dy64->indirectsyms =
        memdup(dy32->indirectsyms, cmd32->nindirectsyms * sizeof(dy32->indirectsyms[0]))) == NULL)
      {
         perror("memdup");
         return -1;
      }

   return 0;
}

static int macho_transform_dyld_info_32to64(const struct dyld_info *dl32, struct dyld_info *dl64) {
   dl64->command = dl32->command;

   if (macho_transform_rebase_info_32to64(dl32->rebase_data, dl32->command.rebase_size,
                                          &dl64->rebase_data, &dl64->command.rebase_size) < 0) {
      return -1;
   }

   if (macho_transform_bind_info_32to64(dl32->bind_data, dl32->command.bind_size,
                                        &dl64->bind_data, &dl64->command.bind_size) < 0) {
      return -1;
   }

   dl64->lazy_bind_data = dl32->lazy_bind_data;
   dl64->export_data = dl32->export_data;

   return 0;
}

/* NOTE: This might not work. */
static int macho_transform_linkedit_32to64(const struct linkedit_data *linkedit32,
                                            struct linkedit_data *linkedit64) {
   linkedit64->command = linkedit32->command;
   linkedit64->data = memdup(linkedit32->data, linkedit32->command.datasize);
   
   return 0; 
}

static int macho_transform_rebase_info_32to64(const void *rebase_data_32, uint32_t rebase_size_32,
                                              void **rebase_data_64, uint32_t *rebase_size_64) {
   /* create duplicate */
   uint8_t *rebase_begin;
   const uint8_t *rebase_end;
   if ((rebase_begin = memdup(rebase_data_64, rebase_size_32)) == NULL) {
      return -1;
   }
   rebase_end = rebase_begin + rebase_size_32;

   for (uint8_t *rebase_it = rebase_begin; rebase_it != rebase_end; ) {
      const uint8_t opcode = *rebase_it & REBASE_OPCODE_MASK;
      const uint8_t imm = *rebase_it & REBASE_IMMEDIATE_MASK;
      size_t uleb_len;
      uint8_t *rebase_instr = rebase_it++; // for subsequent modification
      size_t rebase_rem = rebase_end - rebase_it;

      switch (opcode) {
      case REBASE_OPCODE_DONE:
         goto done;

      case REBASE_OPCODE_SET_TYPE_IMM:
         if (imm != BIND_TYPE_POINTER) {
            fprintf(stderr, "macho_transform_rebase_info_32to64: unsupported rebase type %d\n",
                    imm);
            return -1;
         }
         /* convert to 32-bit absolute address */
         *rebase_instr = REBASE_OPCODE_SET_TYPE_IMM | REBASE_TYPE_TEXT_ABSOLUTE32;
         break;

      case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
         break;

      case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
      case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
      case REBASE_OPCODE_ADD_ADDR_ULEB:
      case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
         uleb_len = uleb128_decode(rebase_it, rebase_rem, NULL);
         if (uleb_len > rebase_rem) {
            fprintf(stderr, "macho_transform_rebase_32to64: rebase data ends inside ULEB\n");
            return -1;
         }
         if (uleb_len == 0) {
            fprintf(stderr, "macho_transform_rebase_32to64: rebase data ULEB overflow\n");
            return -1; 
         }
         rebase_it += uleb_len;
         break;

      default:
         fprintf(stderr, "macho_transform_rebase_32to64: unsupported opcode 0x%x\n", opcode);
         return -1;
      }
   }

 done:
   *rebase_data_64 = rebase_begin;
   *rebase_size_64 = rebase_size_32;

   return 0;
}

static int macho_transform_bind_info_32to64(const void *bind_data_32, uint32_t bind_size_32,
                                            void **bind_data_64, uint32_t *bind_size_64) {
   /* duplicate */
   uint8_t *bind_begin;
   if ((bind_begin = memdup(bind_data_32, bind_size_32)) == NULL) {
      perror("memdup");
      return -1;
   }

   uint8_t *bind_it = bind_begin;
   const uint8_t *bind_end = bind_begin + bind_size_32;

   while (bind_it < bind_end) {
      size_t uleb_len, sleb_len;
      
      const uint8_t opcode = (*bind_it & BIND_OPCODE_MASK);
      const uint8_t imm = (*bind_it & BIND_IMMEDIATE_MASK);

      ++bind_it;

      size_t bind_rem = bind_end - bind_it;
      
      switch (opcode) {
      case BIND_OPCODE_DONE:
         goto done;

      case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
      case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
         // TODO: How to manage reordering?
         break;

      case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
         uleb_len = uleb128_decode(bind_it, bind_rem, NULL);
         if (uleb_len > bind_rem) {
            fprintf(stderr, "macho_transform_bind_info_32to64: bind info ended inside ULEB\n");
            return -1;
         } else if (uleb_len == 0) {
            fprintf(stderr, "macho_transform_bind_info_32to64: ULEB overflow\n");
            return -1;
         }
         bind_it += uleb_len;
         break;

      case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
         /* symbol name follows */
         switch (imm) {
         case 0x0:
            while (*bind_it++ != '\0') {} /* skip symbol string */
            break;

         default:
            fprintf(stderr, "macho_transform_bind_info_32to64: unrecognized trailing flags type 0x%x\n", imm);
            return -1;
         }
         break;

      case BIND_OPCODE_SET_TYPE_IMM:
         switch (imm) {
         case BIND_TYPE_POINTER:
            *(bind_it - 1) = BIND_TYPE_TEXT_ABSOLUTE32;
            break;
         case BIND_TYPE_TEXT_ABSOLUTE32:
            fprintf(stderr,
                    "macho_transform_bind_info_32to64: eek -- encountered BIND_TYPE_TEXT_ABSOLUTE32\n");
            return -1;
         default:
            fprintf(stderr, "macho_transform_bind_info_32to64: unsupported bind type %d\n", imm);
            return -1;
         }
         break;

      case BIND_OPCODE_SET_ADDEND_SLEB:
         sleb_len = sleb128_decode(bind_it, bind_rem, NULL);
         if (sleb_len == 0) {
            fprintf(stderr, "macho_transform_bind_info_32to64: SLEB overflow\n");
            return -1;
         } else if (sleb_len > bind_rem) {
            fprintf(stderr, "macho_transform_bind_info_32to64: bind info ends inside SLEB\n");
            return -1;
         }
         bind_it += sleb_len;
         break;

      case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
      case BIND_OPCODE_ADD_ADDR_ULEB:
      case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
         uleb_len = uleb128_decode(bind_it, bind_rem, NULL);
         if (uleb_len == 0) {
            fprintf(stderr, "macho_transform_bind_info_32to64: ULEB overflow\n");
            return -1;
         } else if (uleb_len > bind_rem) {
            fprintf(stderr, "macho_transform_bind_info_32to64: bind info ends inside ULEB\n");
         }
         bind_it += uleb_len;
         break;

      case BIND_OPCODE_DO_BIND:
         break;

      case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
         {
            uintmax_t ulebs[2];
            size_t uleb_lens[2];

            for (int i = 0; i < 2; ++i) {
               uleb_lens[i] = uleb128_decode(bind_it, bind_rem, &ulebs[i]);
               if (uleb_lens[i] == 0) {
                  fprintf(stderr, "macho_transform_bind_info_32to64: ULEB overflow\n");
                  return -1;
               } else if (uleb_lens[i] > bind_rem) {
                  fprintf(stderr, "macho_transform_bind_info_32to64: bind info ends inside ULEB\n");
                  return -1;
               }
               bind_it += uleb_lens[i];
               bind_rem -= uleb_lens[i];
            }
         }
         break;

      case BIND_OPCODE_THREADED:
         fprintf(stderr, "macho_transform_bind_info_32to64: BIND_OPCODE_THREADED unsupported\n");
         return -1;
         
      default:
         fprintf(stderr, "macho_transform_bind_info_32to64: unrecognized opcode 0x%02x\n", opcode);
         return -1;
      }
   }

 done:
   *bind_size_64 = bind_size_32;
   *bind_data_64 = bind_begin;
   
   return 0;
}
