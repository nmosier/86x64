#include "build.hh"
#include "util.hh"

namespace MachO {

   template <Bits bits>
   std::size_t BuildEnv<bits>::allocate(std::size_t size) {
      if (size == 0) {
         return 0;
      } else {
         std::size_t r_offset = loc.offset;
         loc.offset += size;
         return r_offset;
      }
   }
   
   template <Bits bits>
   void BuildEnv<bits>::allocate(std::size_t size, Location& out_loc) {
      
      out_loc.offset = allocate(size);
      out_loc.vmaddr = loc.vmaddr;
      loc.vmaddr += size;
   }

   template class BuildEnv<Bits::M32>;
   template class BuildEnv<Bits::M64>;
   
}
