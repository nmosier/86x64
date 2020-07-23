#include "asm.hpp"

namespace zc::z80 {

   static const RegisterValue FP_register_value(&r_ix);
   const MemoryValue FP_memval(&FP_register_value, long_size); /* NOTE: long size because it'll store
                                                                * the return address. */
   const IndexedRegisterValue FP_idxval(&rv_ix, (int8_t) 0);
  
}
