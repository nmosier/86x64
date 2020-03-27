#ifndef MACHO_BUILD_H
#define MACHO_BUILD_H

#include <stdio.h>

#include "macho.h"

#if 0
struct build_info {
   union archive *archive;
   macho_off_t curoff; /*!< current file offset */
   macho_off_t dataoff; /*!< data file offset */
   macho_addr_t vmaddr; /*!< current virtual memory address */
};
#endif

struct build_info_32 {
   struct archive_32 *archive;
   uint32_t curoff;
   uint32_t dataoff;
   uint32_t vmaddr;
};

struct build_info_64 {
   struct archive_64 *archive;
   uint64_t curoff;
   uint64_t dataoff;
   uint64_t vmaddr;
};

/**
 * Build Mach-O binary, which involves recomputing all offsets, ...
 * @param macho Mach-O structure to build
 * @return 0 on succes; -1 on error.
 */
int macho_build(union macho *macho);
int macho_build_archive(union archive *archive);
#if 0
int macho_build_archive_32(struct archive_32 *archive, struct build_info *info);
int macho_build_load_command_32(union load_command_32 *command, struct build_info *info);
int macho_build_load_command_64(union load_command_64 *command, struct build_info *info);
int macho_build_segment_32(struct segment_32 *segment, struct build_info *info);
int macho_build_symtab_32(struct symtab_32 *symtab, struct build_info *info);
int macho_build_dysymtab_32(struct dysymtab_32 *dysymtab, struct build_info *info);
int macho_build_dyld_info(struct dyld_info *dyld_info, struct build_info *info);
#endif

#endif
