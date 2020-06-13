#pragma once

namespace MachO {

   enum class Filetype {FAT, ARCHIVE};
   enum class Bits {M32, M64};
   enum class Endianness {SAME, DIFF};

   template <Bits bits> class Archive;
   template <Bits bits> class MachHeader;
   
}

