#pragma once

#include <cstdlib>

#include "types.hh"
#include "loc.hh"

namespace MachO {

   template <Bits bits>
   class BuildEnv {
   public:
      Archive<bits> *archive = nullptr;

      /**
       * Allocates given number of bytes and returns offset of beginning of allocated region.
       * @param size number of bytes
       * @return offset of region start
       */
      std::size_t allocate(std::size_t size);

      /**
       * Allocates given number of bytes, updates offset counter and virtual memory address counter.
       * @param size number of bytes
       * @param loc output location, essentially a virtual address and offset pair
       */
      void allocate(std::size_t size, Location& loc);

      void align(unsigned pow2) { loc.align(pow2); }
      
      void newsegment();

      unsigned segment_counter() { return segment_counter_++; }
      unsigned dylib_counter() { return dylib_counter_++; }
      uint8_t section_counter();
      uint32_t lazy_bind_index(uint32_t size);
      
      Location loc;      

      BuildEnv(Archive<bits> *archive, const Location& loc): archive(archive), loc(loc) {}
      
   private:
      unsigned segment_counter_ = 0;
      uint8_t section_counter_ = 0;
      unsigned dylib_counter_ = 1;
      uint32_t lazy_bind_index_ = 0;
   };

}
