#include "macho-driver.h"

int main(int argc, char *argv[]) {
   return macho_main(argc, argv);
}

static int macho_noop(const union macho *macho_in, union macho *macho_out) {
   memcpy(macho_out, macho_in, sizeof(*macho_in));
   return 0;
}
