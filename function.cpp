extern "C" {
#include <xed/xed-interface.h>
}

#include "function.hpp"

Function::Function(Function::Symbol& symbol, const xed_uint8_t *itext, unsigned int bytes):
   symbol_(symbol)
{
   while (bytes > 0) {
      xed_decoded_inst_t decoded;
      xed_error_enum_t err;
      if ((err = xed_decode(&decoded, itext, bytes)) != XED_ERROR_NONE) {
         throw xed_error_enum_t2str(err);
      }
      insts_.push_back(decoded);
      bytes -= decoded._decoded_length;
      itext += decoded._decoded_length;
   }
}
