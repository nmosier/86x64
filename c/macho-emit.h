#ifndef MACHO_EMIT_H
#define MACHO_EMIT_H

#include <stdio.h>

#include "macho-parse.h"

/**
 * Emit Mach-O fat binary.
 * @param f output file
 * @param macho Mach-O structure
 */
int macho_emit(FILE *f, const union macho *macho);
int macho_emit_archive(FILE *f, const union archive *archive);
int macho_emit_archive_32(FILE *f, const struct archive_32 *archive);
int macho_emit_load_command_32(FILE *f, const union load_command_32 *command);

#endif
