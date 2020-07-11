#pragma once

#include <vector>
#include <xed-interface.h>

#include "types.hh"

namespace MachO {

   typedef std::vector<uint8_t> opcode_t;
   
   namespace opcode {

      template <Bits bits>
      int32_t disp32(const SectionBlob<bits> *src, const SectionBlob<bits> *dst,
                     std::size_t new_src_len);
      

      /* push r11 */
      inline opcode_t push_r11() { return {0x41, 0x53}; }
      
      /* lea r11, [rip + disp32] */
      /* NOTE: Disp is from beginning */
      opcode_t lea_r11_mem_rip_disp32(int32_t disp);

      
      opcode_t lea_r11_mem_rip_disp32(const SectionBlob<Bits::M32> *src,
                                      const SectionBlob<Bits::M32> *dst);
      
   }

}
