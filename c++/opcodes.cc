#include "opcodes.hh"

namespace MachO::opcode {

   namespace {

      template <typename It, typename T>
      It encode_int(It it, T val) {
         static_assert(std::is_integral<T>());
         for (std::size_t i = 0; i < sizeof(val); ++i, val >>= 8) {
            *it++ = val & 0xff;
         }
         return it;
      }

      template <typename T>
      void encode_int(opcode_t& opcode, T val) {
         encode_int(std::back_inserter(opcode), val);
      }
      
   }
   
   opcode_t lea_r11_mem_rip_disp32(int32_t disp) {
      opcode_t opcode {0x4c, 0x8d, 0x1d};
      encode_int(opcode, disp);
      return opcode;
   }
   
}
