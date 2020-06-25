#pragma once

#include <cstdlib>

namespace MachO {
   
   struct Location {
      std::size_t offset;
      std::size_t vmaddr;
      
      Location(): offset(0), vmaddr(0) {}
      Location(std::size_t offset, std::size_t vmaddr): offset(offset), vmaddr(vmaddr) {}

      void align(unsigned pow2);
   };   
   
}
