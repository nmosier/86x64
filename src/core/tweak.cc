#include <mach-o/fat.h>

#include "tweak.hh"
#include "error.hh"

namespace MachO::Tweak {

   MachO *MachO::Parse(Image& img) {
      switch (img.at<uint32_t>(0)) {
      case MH_MAGIC: return Archive<Bits::M32>::Parse(img);
      case MH_MAGIC_64: return Archive<Bits::M64>::Parse(img);

      case MH_CIGAM:
      case MH_CIGAM_64:
         throw unsupported_format("archive is of opposite endianness");

      case FAT_MAGIC:
      case FAT_MAGIC_64:
      case FAT_CIGAM:
      case FAT_CIGAM_64:
         throw unsupported_format("fat archives currently unsupported");

      default:
         throw unrecognized_format("bad magic number");
      }
   }
   
}
