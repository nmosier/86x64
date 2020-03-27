#ifndef MACHO_PATCH_H
#define MACHO_PATCH_H

#include "macho.h"

int macho_patch(union macho *macho);
int macho_patch_archive(union archive *archive);
int macho_patch_archive_32(struct archive_32 *archive);

int macho_patch_TEXT(struct segment_32 *text);
macho_addr_t macho_patch_TEXT_address(macho_addr_t addr, const struct segment_32 *seg);

int macho_patch_DATA(struct segment_32 *data_seg, const struct segment_32 *text_seg);
macho_addr_t macho_patch_symbol_pointer(macho_addr_t addr, const struct segment_32 *text);

int macho_patch_dyld_info_32(struct dyld_info *dyld, struct archive_32 *archive);
int macho_patch_dyld_info_64(struct dyld_info *dyld, struct archive_64 *archive);

#endif
