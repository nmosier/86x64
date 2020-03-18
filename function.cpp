extern "C" {
#include <xed/xed-interface.h>
}

#include "function.hpp"

DecodedFunction::content_t DecodedFunction::content_from_itext(const xed_uint8_t *itext,
                                                               unsigned int bytes)
{
   content_t insts;
   while (bytes > 0) {
      xed_decoded_inst_t decoded;
      xed_error_enum_t err;
      if ((err = xed_decode(&decoded, itext, bytes)) != XED_ERROR_NONE) {
         throw xed_error_enum_t2str(err);
      }
      insts.push_back(decoded);
      bytes -= decoded._decoded_length;
      itext += decoded._decoded_length;
   }

   return insts;
}

DecodedFunction::content_t DecodedFunction::content_from_binary(const Symbol& symbol,
                                                                const LIEF::MachO::Binary& binary)
{
   // get containing section
   uint64_t sym_off = symbol.value();
   uint64_t sym_size = symbol.size();
   const LIEF::MachO::Section *sym_section = binary.section_from_offset(sym_off);
   assert(sym_section);

   // find offset of symbol content within section
   assert(sym_off >= sym_section->offset());
   assert(sym_size <= sym_section->size());
   assert(sym_off + sym_size <= sym_section->offset() + sym_section->size());
   uint64_t sym_sect_off = sym_off - sym_section->offset();
   xed_uint8_t *sym_content = &*sym_section->content().begin() + sym_sect_off;

   return content_from_itext(sym_content, sym_size);
}
