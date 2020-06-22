#pragma once

#include <cstdlib>

#include "macho-fwd.hh"
#include "loc.hh"

namespace MachO {

   template <Bits bits>
   class BuildEnv {
   public:
      Archive<bits>& archive;

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
      
      void newsegment();

      template <typename T>
      unsigned counter() const {
         static unsigned count = 0;
         return ++count;
      }
         
#if 0
      unsigned register_dylib() { return ++dylib_counter; }
#endif
      
      const Location& loc() const { return loc_; }
      
      BuildEnv(Archive<bits>& archive): archive(archive) {}
      
   private:
      Location loc_;
#if 0
      unsigned dylib_counter;
#endif
   };
   
}
