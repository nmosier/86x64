#ifndef MACHO_PARSE_H
#define MACHO_PARSE_H

#include <stdio.h>

#include "macho.h"


/* NOTE: Currently assumes endianness is same as machine this program is running on, 
 * i.e. it doesn't adjust endianness.
 */
int macho_parse(FILE *f, union macho *macho);

/**
 * Parse fat Mach-O file.
 * @param f input stream
 * @param magic magic value to initialize with if already read, or NULL to read value from stream
 * @param fat output Mach-O fat structure
 * @return 0 on success; -1 on error.
 */
int macho_parse_fat(FILE *f, const uint32_t *magic, struct fat *fat);

/**
 * Parse Mach-O archive.
 * @param f input stream
 * @param magic magic value to initialize with if already read, or NULL to read value from stream
 * @param archive output Mach-O archive structure
 * @return 0 on success; -1 on error.
 */
int macho_parse_archive(FILE *f, const uint32_t *magic, union archive *archive);

/**
 * Parse 32-bit Mach-O archive.
 * @param f input stream
 * @param magic magic value to initialize with if already read, or NULL to read value from stream
 * @param archive header output 32-bit Mach-O header
 */
int macho_parse_archive_32(FILE *f, const uint32_t *magic, struct archive_32 *archive);

/**
 * Parse 32-bit load command.
 * @param f input stream
 * @param command output load command structure
 * @param dataoff absolute file offset of data in archive
 */
int macho_parse_load_command_32(FILE *f, union load_command_32 *command);
int macho_parse_load_command_64(FILE *f, union load_command_64 *command);

#endif

