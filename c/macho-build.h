#ifndef MACHO_BUILD_H
#define MACHO_BUILD_H

#include <stdio.h>

#include "macho-parse.h"

typedef off_t macho_off_t;
typedef uint64_t macho_addr_t;

struct build_info {
   union archive *archive;
   macho_off_t curoff; /*!< current file offset */
   macho_off_t dataoff; /*!< data file offset */
   macho_addr_t vmaddr; /*!< current virtual memory address */
};

/**
 * Build Mach-O binary, which involves recomputing all offsets, ...
 * @param macho Mach-O structure to build
 * @return 0 on succes; -1 on error.
 */
int macho_build(union macho *macho);
int macho_build_archive(union archive *archive, struct build_info *info);
int macho_build_archive_32(struct archive_32 *archive, struct build_info *info);
int macho_build_load_command_32(union load_command_32 *command, struct build_info *info);
int macho_build_segment_32(struct segment_32 *segment, struct build_info *info);
int macho_build_symtab_32(struct symtab_32 *symtab, struct build_info *info);
int macho_build_dysymtab_32(struct dysymtab_32 *dysymtab, struct build_info *info);

#endif
