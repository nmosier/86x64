#include <mach-o/fat.h>
#include <mach-o/loader.h>

extern "C" {
#include <xed-interface.h>
}

#include "archive.hh"

namespace MachO {
   
   void init() {
      xed_tables_init();
   }

   MachO *MachO::Parse(const Image& img) {
      switch (img.at<uint32_t>(0)) {
      case MH_MAGIC:    return Archive<Bits::M32>::Parse(img);
      case MH_MAGIC_64: return Archive<Bits::M64>::Parse(img);
         
      case MH_CIGAM:
      case MH_CIGAM_64:
      case FAT_MAGIC:
      case FAT_CIGAM:
      case FAT_MAGIC_64:
      case FAT_CIGAM_64:
      default:
         throw error("opposite endianness unsupported");
      }

      return NULL;
   }
   
}
