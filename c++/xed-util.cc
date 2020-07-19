#include "types.hh"
#include "image.hh"

namespace MachO::xed {

#if 0
   template <Bits bits>
   xed_iform_enum_t get_iform(const Image& img, std::size_t offset) {
      xed_decoded_inst_t xedd;
      xed_decoded_inst_zero_set_mode(&xedd, &Instruction<bits>::dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
      
      xed_error_enum_t err;
      if ((err = xed_decode(&xedd, &img.at<uint8_t>(offset), img.size() - offset)) !=
          XED_ERROR_NONE) {
         throw error("%s: bad instruction at image offset 0x%zx", __FUNCTION__, offset);
      }

      return xed_decoded_inst_get_iform_enum(&xedd);
   }
#endif

   template <Bits bits>
   void decode(const Image& img, std::size_t offset, xed_decoded_inst_t& xedd) {
      xed_decoded_inst_zero_set_mode(&xedd, &Instruction<bits>::dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
      
      xed_error_enum_t err;
      if ((err = xed_decode(&xedd, &img.at<uint8_t>(offset), img.size() - offset)) !=
          XED_ERROR_NONE) {
         throw error("%s: bad instruction at image offset 0x%zx", __FUNCTION__, offset);
      }
   }

}
