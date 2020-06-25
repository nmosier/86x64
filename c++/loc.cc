#include "loc.hh"
#include "util.hh"

namespace MachO {

   void Location::align(unsigned pow2) {
      offset = align_up<std::size_t>(offset, 1 << pow2);
      vmaddr = align_up<std::size_t>(vmaddr, 1 << pow2);
   }

}
