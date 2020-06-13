#include <string.h>

#include "macho-driver.h"

static int macho_noop(const union macho *macho_in, union macho *macho_out);

int main(int argc, char *argv[]) {
   return macho_main(argc, argv, macho_noop);
}

static int macho_noop(const union macho *macho_in, union macho *macho_out) {
   memcpy(macho_out, macho_in, sizeof(*macho_in));
   return 0;
}
