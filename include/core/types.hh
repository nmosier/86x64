#pragma once

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "util.hh"

namespace MachO {
   
   template <Bits bits>
   using mach_header_t = select_type<bits, mach_header, mach_header_64>;

   template <Bits bits>
   using segment_command_t = select_type<bits, segment_command, segment_command_64>;

   template <Bits bits>
   using nlist_t = select_type<bits, struct nlist, struct nlist_64>;

   template <Bits bits>
   using section_t = select_type<bits, struct section, struct section_64>;

   template <Bits bits>
   using fat_arch_t = select_type<bits, struct fat_arch, struct fat_arch_64>;

   template <Bits bits>
   using ptr_t = select_type<bits, uint32_t, uint64_t>;

   template <Bits bits> using macho_addr_t = select_type<bits, uint32_t, uint64_t>;
   template <Bits bits> using macho_size_t = macho_addr_t<bits>;

   class Image;
   
   template <Bits> class ParseEnv;
   template <Bits b1, Bits b2 = opposite<b1>> class TransformEnv;
   template <Bits> class BuildEnv;
   
   class MachO;
   template <Bits> class Fat;
   template <Bits> class Archive;

   template <Bits> class LoadCommand;
   template <Bits> class DylibCommand;
   template <Bits> class Segment;
   template <Bits> class LinkeditCommand;
   template <Bits> class DyldInfo;
   template <Bits> class FunctionStarts;
   template <Bits> class DataInCode;
   template <Bits> class Symtab;
   template <Bits> class Dysymtab;
   template <Bits> class CodeSignature;
   
   template <Bits> class Instruction;
   template <Bits> class SectionBlob;
   template <Bits> class Placeholder;
   template <Bits> class RelocBlob;

   template <Bits> class RebaseInfo;
   template <Bits, bool> class BindInfo;
   template <Bits> class ExportInfo;

   template <Bits, bool> class BindNode;
   template <Bits b> using LazyBindNode = BindNode<b, true>;
   
   enum class Relation {BEFORE, AFTER};   
   
   constexpr std::size_t PAGESIZE = 0x1000;

   template <Bits bits>
   constexpr std::size_t vmaddr_start = bits == Bits::M32 ? 0x1000 : 0x100000000;

   class Node {
   private:
      virtual void dummy() const {}
   };

}
