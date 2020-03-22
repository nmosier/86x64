#ifndef MACHO_UTIL_H
#define MACHO_UTIL_H

#include <stdbool.h>

#include "macho-parse.h"

enum macho_kind {MACHO_FAT, MACHO_ARCHIVE};
enum macho_kind macho_kind(const union macho *macho);

enum macho_bits {MACHO_32, MACHO_64};
enum macho_bits macho_bits(const union archive *archive);

bool macho_is_linkedit(const union load_command_32 *command);

#endif
