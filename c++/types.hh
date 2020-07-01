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
   using ptr_t = select_type<bits, uint32_t, uint64_t>;

   class Image;
   
   template <Bits> class ParseEnv;
   template <Bits b1, Bits b2 = opposite<b1>> class TransformEnv;
   template <Bits> class BuildEnv;
   
   class MachO;
   class Fat;
   template <Bits> class Archive;

   template <Bits> class LoadCommand;
   template <Bits> class DylibCommand;
   template <Bits> class Segment;
   template <Bits> class LinkeditCommand;
   
   template <Bits> class Instruction;
   template <Bits> class SectionBlob;
   
}
