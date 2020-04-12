#include <stdlib.h>
#include <string.h>
#include <mach/machine.h>

#include "macho-transform.h"

static int macho_transform_command_32to64(const union load_command_32 *cmd32,
                                          union load_command_64 *cmd64);
static int macho_transform_segment_32to64(const struct segment_32 *seg32,
                                          struct segment_64 *seg64);
static int macho_transform_section_32to64(const struct section_wrapper_32 *wr32,
                                          struct section_wrapper_64 *wr64);
static int macho_transform_symtab_32to64(const struct symtab_32 *symtab32,
                                         struct symtab_64 *symtab64);
static int macho_transform_nlist_32to64(const struct nlist *ent32, struct nlist_64 *ent64);
static int macho_transform_dysymtab_32to64(const struct dysymtab_32 *dy32,
                                           struct dysymtab_64 *dy64);

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

   /* allocate 64-bit commands */
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
      abort();

   case LC_UUID:
      cmd64->uuid = cmd32->uuid;
      break;

   case LC_UNIXTHREAD:

   case LC_CODE_SIGNATURE:
   case LC_FUNCTION_STARTS:
   case LC_DATA_IN_CODE:

   case LC_DYLD_INFO:
   case LC_DYLD_INFO_ONLY:
      abort();

   case LC_VERSION_MIN_MACOSX:
      cmd64->version_min = cmd32->version_min;
      break;

   case LC_SOURCE_VERSION:
      cmd64->source_version = cmd32->source_version;
      break;

   case LC_MAIN:
      abort();

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
   
   // TODO
   abort();
}

static int macho_transform_segment_32to64(const struct segment_32 *seg32, struct segment_64 *seg64)
{
   const struct segment_command *cmd32 = &seg32->command;
   struct segment_command_64 *cmd64 = &seg64->command;
   cmd64->cmd      = cmd32->cmd;
   cmd64->cmdsize  = cmd32->cmdsize;
   memcpy(cmd64->segname, cmd32->segname, sizeof(cmd64->segname));
   cmd64->vmaddr   = cmd32->vmaddr;
   cmd64->vmsize   = cmd32->vmsize;
   cmd64->fileoff  = cmd32->fileoff;
   cmd64->filesize = cmd32->filesize;
   cmd64->maxprot  = cmd32->maxprot;
   cmd64->initprot = cmd32->initprot;
   cmd64->nsects   = cmd32->nsects;
   cmd64->flags    = cmd32->flags;

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

static int macho_transform_dysymtab_32to64(const struct dysymtab_32 *dy32,
                                           struct dysymtab_64 *dy64) {
   const struct dysymtab_command *cmd32 = &dy32->command;
   struct dysymtab_command *cmd64 = &dy64->command;
   *cmd64 = *cmd32;

   if ((dy64->indirectsyms =
        calloc(cmd64->nindirectsyms, sizeof(dy64->indirectsyms[0]))) == NULL) {
      perror("calloc");
      return -1;
   }

   for (uint32_t i = 0; i < cmd64->nindirectsyms; ++i) {
      dy64->indirectsyms[i] = dy32->indirectsyms[i];
   }

   return 0;
}
