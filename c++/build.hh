#pragma once

#include "macho-fwd.hh"

namespace MachO {

   template <Bits bits>
   class BuildEnv {
   public:
      std::size_t allocate(std::size_t size);
      void newpage();

      BuildEnv(): vmaddr(0), offset(0) {}
      
   private:
      std::size_t vmaddr;
      std::size_t offset;
   };
   
}
