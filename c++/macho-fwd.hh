#pragma once

namespace MachO {

   enum class Filetype {FAT, ARCHIVE};
   enum class Bits {M32, M64};
   enum class Endianness {SAME, DIFF};

   class Image;
   class MachO;
   class Fat;
   template <Bits bits> class Archive;
   template <Bits bits> class MachHeader;
   template <Bits bits> class LoadCommand;
   template <Bits bits> class Segment;
   template <Bits bits> class Section;
   template <Bits bits> class DyldInfo;
   template <Bits bits> class Symtab;
   template <Bits bits> class Nlist;
   template <Bits bits> class Dysymtab;
   template <Bits bits> class DylinkerCommand;
   template <Bits bits> class UUID;
   template <Bits bits> class BuildVersion;
   template <Bits bits> class BuildToolVersion;
   template <Bits bits> class Instruction;
   template <Bits bits> class DataInCode;
   template <Bits bits> class DataInCodeEntry;
   
}

