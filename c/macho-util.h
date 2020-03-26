#ifndef MACHO_UTIL_H
#define MACHO_UTIL_H

#include <stdbool.h>

#include "macho.h"

enum macho_kind macho_kind(const union macho *macho);

enum macho_bits macho_bits(const union archive *archive);

bool macho_is_linkedit_32(const union load_command_32 *command);
bool macho_is_linkedit_64(const union load_command_64 *command);

/**
 * Return linkedit command if found.
 * @param command load command
 * @return pointer to linkedit command if found; NULL otherwise.
 */
struct linkedit_data *macho_linkedit(union load_command_32 *command);

struct segment_32 *macho_find_segment_32(const char *segname, struct archive_32 *archive);
struct section_wrapper_32 *macho_find_section_32(const char *sectname, struct segment_32 *segment);

union load_command_32 *macho_find_load_command_32(uint32_t cmd, struct archive_32 *archive);

int macho_off_cmp(const macho_off_t *a, const macho_off_t *b);

struct segment_32 *macho_index_segment_32(uint32_t index, struct archive_32 *archive);

#endif
