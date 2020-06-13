#ifndef MACHO_INSERT_H
#define MACHO_INSERT_H

#include <stdint.h>

int macho_insert_instruction(const void *instbuf, size_t instlen, uint64_t vmaddr,
                             union archive *archive);

#endif
