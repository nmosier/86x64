/* Translate 32-bit memory instructions to 64-bit memory instructions */

#include <algorithm>

#include <LIEF/LIEF.hpp>
#include "instrs.hpp"

void transform_instructions(std::shared_ptr<LIEF::MachO::FatBinary> macho) {
   for (auto&& binary : *macho) {
      // construct a table of symbol names
#if 0
      auto section = binary.
      
      
      auto&& sections = binary.sections();
      auto text_section = std::find_if(sections.begin(), sections.end(),
                                       [](auto it){ return it.name() == "__text"; });
      if (text_section == sections.end()) {
         throw "binary is missing __text section";
      }

      // iterate thru instructions
      auto text_content = text_section->content();
      std::cout << text_content.size() << std::endl;
#endif

      for (auto symbol : binary.symbols()) {
         std::cout << symbol.name() << " " << symbol.value() << std::endl;
      }
   }
}
