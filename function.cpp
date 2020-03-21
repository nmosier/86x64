extern "C" {
#include <xed/xed-interface.h>
}

#include "function.hpp"

DecodedFunction::content_t DecodedFunction::content_from_itext(const xed_uint8_t *itext,
                                                               uint64_t bytes)
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

DecodedFunction::content_t DecodedFunction::content_from_addr(uint64_t addr, uint64_t bytes,
                                                              const Binary& binary)
{
   // find text section
   const Section& text_section = binary.get_section("__text");

   const std::vector<uint8_t> content =
      binary.get_content_from_virtual_address(addr + text_section.address(),
                                              bytes);
   const xed_uint8_t *itext = &*content.begin();
   return content_from_itext(itext, bytes);
}

DecodedFunctions::DecodedFunctions(const Binary& binary) {
   // sort all functions based on start address
   auto starts = binary.function_starts().functions();
   std::set<uint64_t> sorted_addrs(starts.begin(), starts.end());

   // construct map from symbol start address to name
   std::map<uint64_t, DecodedFunction::Symbol *> func_map;
   for (auto& sym : binary.symbols()) {
      if (!sym.is_external()) {
         func_map[sym.value()] = &sym;
      }
   }
   
   auto it = sorted_addrs.rbegin();
   auto text = binary.get_section("__text");
   // assert(text.has_segment());
   const LIEF::MachO::SegmentCommand& TEXT = text.segment();
   uint64_t end_addr = text.virtual_address() + text.size();
   while (it != sorted_addrs.rend()) {
      assert(end_addr >= *it + TEXT.virtual_address());
      auto sym_it = func_map.find(*it);
      functions_.emplace_back(sym_it == func_map.end() ? std::nullopt : std::make_optional(sym_it->second),
                              TEXT.virtual_address() + *it, end_addr - (TEXT.virtual_address() + *it), binary);
      end_addr = *it++;
   }
}
