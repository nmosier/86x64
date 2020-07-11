#pragma once

#include <vector>
#include <xed-interface.h>

#include "types.hh"

namespace MachO::opcode {

   typedef std::vector<uint8_t> opcode_t;

   /* push r11 */
   opcode_t push_r11() { return {0x41, 0x53}; }

   /* lea r11, [rip + disp32] */
   opcode_t lea_r11_mem_rip_disp32(int32_t disp);
   

}
