#include <string.h>
#include <assert.h>
#include <scanopt.h>
#include <getopt.h>

#include "macho-driver.h"
#include "macho-insert.h"
#include "macho.h"
#include "macho-util.h"
#include "macho-build.h"

static int macho_insert(const union macho *macho_in, union macho *macho_out);

static unsigned long g_offset = (unsigned long) -1;
static const char *g_segment = NULL;
static const char *g_section = NULL;

int main(int argc, char *argv[]) {
   const char *usage =
      "usage: %1%s -S segment -s section -o offset inpath [outpath]\n" \
      "       %1$s -h\n";
   
   int help;
   
   if (scanopt(argc, argv, "S:s:o+h", &g_segment, &g_section, &g_offset, &help) < 0) {
      fprintf(stderr, usage, argv[0]);
      return 1;
   }

   if (help) {
      printf(usage, argv[0]);
      return 0;
   }

   if (g_segment == NULL) {
      fprintf(stderr, "%s: -S: segment missing\n", argv[0]);
      return 1;
   }

   if (g_section == NULL) {
      fprintf(stderr, "%s: -s: section missing\n", argv[0]);
      return 1;
   }
   
   if (g_offset == (unsigned long) -1) {
      fprintf(stderr, "%s: -o: invalid vmaddr 0x%lx\n", argv[0], g_offset);
      return 1;
   }

   argv[optind - 1] = argv[0];

   return macho_main(argc - optind + 1, &argv[optind - 1], macho_insert);
}

#define MAX_INST_LEN 15

static int macho_insert(const union macho *macho_in, union macho *macho_out) {
   assert(macho_kind(macho_in) == MACHO_ARCHIVE);

   memcpy(macho_out, macho_in, sizeof(*macho_in));

   /* find segment */
   uint64_t vmaddr;
   switch (macho_bits(&macho_out->archive)) {
   case MACHO_32:
      {
         struct segment_32 *segment;
         if ((segment = macho_find_segment_32(g_segment, &macho_out->archive.archive_32)) == NULL) {
            fprintf(stderr, "macho_find_segment_32: segment '%s' not found\n", g_segment);
            return -1;
         }

         struct section_wrapper_32 *sectwr;
         if ((sectwr = macho_find_section_32(g_section, segment)) == NULL) {
            fprintf(stderr, "macho_find_segment_32: section '%s' not found\n", g_section);
         }

         vmaddr = sectwr->section.addr + g_offset;
      }
      break;
      
   case MACHO_64:
      {
         struct segment_64 *segment;
         if ((segment = macho_find_segment_64(g_segment, &macho_out->archive.archive_64)) == NULL) {
            fprintf(stderr, "macho_find_segment_64: segment '%s' not found\n", g_segment);
            return -1;
         }

         struct section_wrapper_64 *sectwr;
         if ((sectwr = macho_find_section_64(g_section, segment)) == NULL) {
            fprintf(stderr, "macho_find_segment_64: section '%s' not found\n", g_section);
         }

         vmaddr = sectwr->section.addr + g_offset;
      }
      break;
   }

   /* read bytes from stdin */
   uint8_t instbuf[MAX_INST_LEN];
   size_t instlen = fread(instbuf, 1, MAX_INST_LEN, stdin);

   if (instlen < MAX_INST_LEN && ferror(stdin)) {
      perror("fread");
      return -1;
   }

   if (macho_insert_instruction(instbuf, instlen, vmaddr, &macho_out->archive) < 0) {
      return -1;
   }

   if (macho_build(macho_out) < 0) {
      return -1;
   }

   /* try not patching? */
   /*
   if (macho_patch(macho_out) < 0) {
      return -1; 
   }
   */

   return 0;
}
