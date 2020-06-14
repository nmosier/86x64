#pragma once

namespace MachO {

   enum class Filetype {FAT, ARCHIVE};
   enum class Bits {M32, M64};
   enum class Endianness {SAME, DIFF};

   class MachO;
   class Fat;
   template <Bits bits> class MachO_;
   template <Bits bits> class LoadCommand;
   
}

