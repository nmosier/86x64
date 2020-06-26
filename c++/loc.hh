#pragma once

#include <cstdlib>
#include <string>

#include "macho-fwd.hh"

namespace MachO {
   
   struct Location {
      std::size_t offset;
      std::size_t vmaddr;
      
      Location(): offset(0), vmaddr(0) {}
      Location(std::size_t offset, std::size_t vmaddr): offset(offset), vmaddr(vmaddr) {}

      void align(unsigned pow2);
   };

   template <Bits bits> class Segment;
   template <Bits bits> class Section;
   
   template <Bits bits>
   struct SectionLocation {
   public:
      Segment<bits> *segment;
      Section<bits> *section;
      std::size_t index;

      SectionLocation(): segment(nullptr), section(nullptr), index(0) {}
      SectionLocation(Segment<bits> *segment, Section<bits> *section, std::size_t index):
         segment(segment), section(section), index(index) {}
   };

}
