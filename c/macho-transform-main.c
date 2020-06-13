#include "macho-driver.h"
#include "macho-build.h"
#include "macho-transform.h"
#include "macho-patch.h"

static int macho_transform(const union macho *macho32, union macho *macho64);

int main(int argc, char *argv[]) {
   return macho_main(argc, argv, macho_transform);
}

static int macho_transform(const union macho *macho32, union macho *macho64) {
   macho64->magic = MH_CIGAM_64;
   
   if (macho_transform_archive_32to64(&macho32->archive.archive_32,
                                      &macho64->archive.archive_64) < 0) {
      return -1;
   }

   /* rebuild */
   if (macho_build(macho64) < 0) {
      return -1;
   }

   /* repatch */
   if (macho_patch(macho64) < 0) {
      return -1;
   }

   return 0;
}
