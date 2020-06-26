#ifndef MACHO_DRIVER_H
#define MACHO_DRIVER_H

#include "macho.h"

typedef int (*macho_op)(const union macho *macho_in, union macho *macho_out);

int macho_main(int argc, char *argv[], macho_op op);

#endif
