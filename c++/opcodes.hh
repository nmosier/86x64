#pragma once

#include <vector>
extern "C" {
#include <xed-interface.h>
}

#include "types.hh"

namespace MachO {

   typedef std::vector<uint8_t> opcode_t;
   
   namespace opcode {

      inline xed_reg_enum_t r32_to_r64(xed_reg_enum_t r32) {
         return (xed_reg_enum_t) (r32 - XED_REG_EAX + XED_REG_RAX);
      }

      template <Bits bits>
      int32_t disp32(const SectionBlob<bits> *src, const SectionBlob<bits> *dst,
                     std::size_t new_src_len);
      

      /* push r11 */
      inline opcode_t push_r11() { return {0x41, 0x53}; }
      
      /* lea r11, [rip + disp32] */
      /* NOTE: Disp is from beginning */
      opcode_t lea_r11_mem_rip_disp32(int32_t disp);
      inline opcode_t lea_r11_mem_rip_disp32() { return {0x4c, 0x8d, 0x1d, 0x00, 0x00, 0x00, 0x00}; }
      
      opcode_t lea_r11_mem_rip_disp32(const SectionBlob<Bits::M32> *src,
                                      const SectionBlob<Bits::M32> *dst);

      inline opcode_t lea_rsp_mem_rsp_4() { return {0x48, 0x8D, 0x64, 0x24, 0x04}; }

      opcode_t lea_r32_mem_rip_disp32(xed_reg_enum_t r32);

      /* jmp [rip+disp32] */
      inline opcode_t jmp_mem_rip_disp32() { return {0xff, 0x25, 0x00, 0x00, 0x00, 0x00}; }

      /* push ax */
      inline opcode_t push_ax() { return {0x66, 0x50}; }

      /* pop r11w */
      inline opcode_t pop_r11w() { return {0x66, 0x41, 0x5B}; }
      
      /* mov [rsp], r32 */
      opcode_t mov_mem_rsp_r32(xed_reg_enum_t r32);

      /* mov r32, [rsp] */
      opcode_t mov_r32_mem_rsp(xed_reg_enum_t r32);

      /* mov eax, imm32 */
      inline opcode_t mov_eax_imm32() { return {0xb8, 0x00, 0x00, 0x00, 0x00}; }

      /* jmp r64 */
      opcode_t jmp_r64(xed_reg_enum_t r64);
      
   }

}
