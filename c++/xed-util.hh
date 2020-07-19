#pragma once

extern "C" {
   #include <xed-interface.h>
}
#include "types.hh"
#include "image.hh"

namespace MachO::xed {

   template <Bits bits>
   bool decode(const Image& img, std::size_t offset, xed_decoded_inst_t& xedd) {
      xed_decoded_inst_zero_set_mode(&xedd, &Instruction<bits>::dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
      return xed_decode(&xedd, &img.at<uint8_t>(offset), img.size() - offset) == XED_ERROR_NONE;
   }
   
}
