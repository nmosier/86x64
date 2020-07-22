#pragma once

#include <string>
extern "C" {
#include <xed/xed-interface.h>
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

   std::string disas(const xed_decoded_inst_t& xedd, std::size_t vmaddr = 0) {
      char pbuf[32];
      xed_print_info_t pinfo;
      xed_init_print_info(&pinfo);
      pinfo.blen = 32;
      pinfo.buf = pbuf;
      pinfo.p = &xedd;
      pinfo.runtime_address = vmaddr;
      xed_format_generic(&pinfo);
      return pbuf;
   }
   
}
