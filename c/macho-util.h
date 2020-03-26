#ifndef MACHO_UTIL_H
#define MACHO_UTIL_H

#include <stdbool.h>

#include "macho.h"

enum macho_kind macho_kind(const union macho *macho);

enum macho_bits macho_bits(const union archive *archive);

bool macho_is_linkedit(const union load_command_32 *command);

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

/**********************
 * CONVENIENCE MACROS *
 **********************/

/**
 * Get the sizeof a struct/union field that depends on the bitness of the Mach-O binary. 
 * Expects suffixes of _32 and _64.
 * @param prefix struct/union lvalue prefix, to which _32 or _64 can be appended
 * @param bits a macho_bits enumeation value
 * @return the sizeof the bits-specific field
 */
#define MACHO_SIZEOF(prefix, bits)                                      \
   (((bits) == MACHO_32) ? sizeof(prefix##_32) : sizeof(prefix##_64))

#define MACHO_SIZEOF_SUFFIX(prefix, bits, suffix)  \
   (((bits) == MACHO_32) ? sizeof(prefix##_32##suffix) : sizeof(prefix##64_##suffix))


#endif
