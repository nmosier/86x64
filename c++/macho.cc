#include <mach-o/fat.h>
#include <mach-o/loader.h>

extern "C" {
#include <xed-interface.h>
}

#include "archive.hh"
#include "fat.hh"
#include "error.hh"

namespace MachO {
   
   void init() {
      xed_tables_init();
   }

   MachO *MachO::Parse(const Image& img) {
      switch (img.at<uint32_t>(0)) {
      case MH_MAGIC:     return Archive<Bits::M32>::Parse(img);
      case MH_MAGIC_64:  return Archive<Bits::M64>::Parse(img);
                         
      case FAT_MAGIC:    return Fat<Bits::M32>::Parse(img);
      case FAT_MAGIC_64: return Fat<Bits::M64>::Parse(img);
         
      case MH_CIGAM:
      case MH_CIGAM_64:
      case FAT_CIGAM:
      case FAT_CIGAM_64:
         throw unsupported_format("archive endianness differs from host endianness");

      default:
         throw unrecognized_format("bad magic number 0x%x", (std::size_t) img.at<uint32_t>(0));
      }
   }
   
}
