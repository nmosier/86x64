#pragma once

#include <cstdlib>
#include <string>
#include <iostream>

#include "macho-fwd.hh"

namespace MachO {
   
   struct Location {
      std::size_t offset;
      std::size_t vmaddr;
      
      Location(): offset(0), vmaddr(0) {}
      Location(std::size_t offset, std::size_t vmaddr): offset(offset), vmaddr(vmaddr) {}

      template <typename T>
      Location operator+(T off) const {
         static_assert(std::is_integral<T>());
         return Location(offset + off, vmaddr + off);
      }
      Location& operator++() { ++offset; ++vmaddr; return *this; }
      template <typename T>
      Location& operator+=(T addend) { offset += addend; vmaddr += addend; return *this; }

      bool operator==(const Location& other) const {
         return offset == other.offset && vmaddr == other.vmaddr;
      }
      bool operator!=(const Location& other) const { return !(*this == other); }

      bool operator<(const Location& other) const {
         return offset < other.offset && vmaddr < other.vmaddr;
      }
      bool operator>(const Location& other) const {
         return offset > other.offset && vmaddr > other.vmaddr;
      }

      template <typename T> Location operator-(T off) const { return operator+(-off); }

      void align(unsigned pow2);
   };

   std::ostream& operator<<(std::ostream& os, const Location& loc);

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
