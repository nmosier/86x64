#ifndef MACHO_SIZEOF_H
#define MACHO_SIZEOF_H

#include "macho-parse.h"

uint32_t macho_sizeof_load_command_32(union load_command_32 *command);
uint32_t macho_sizeof_segment_32(struct segment_32 *segment);
uint32_t macho_sizeof_dylinker(struct dylinker *dylinker);
uint32_t macho_sizeof_thread(struct thread *thread);
uint32_t macho_sizeof_dylib(struct dylib_wrapper *dylib);

#endif
