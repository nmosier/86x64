#include "section.hh"

namespace MachO {

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset) {
      section_t sect = img.at<section_t>(offset);
      uint32_t flags = sect.flags;
      
      if ((flags & S_ATTR_PURE_INSTRUCTIONS)) {
         return TextSection<bits>::Parse(img, offset);
      } else if ((flags & S_ATTR_SOME_INSTRUCTIONS)) {
         throw error("segments with only 'some' instructions not supported");
      } else if (flags == S_NON_LAZY_SYMBOL_POINTERS || flags == S_LAZY_SYMBOL_POINTERS) {
         return SymbolPointerSection<bits>::Parse(img, offset);
      } else if (flags == S_REGULAR || flags == S_CSTRING_LITERALS) {
         return DataSection<bits>::Parse(img, offset);
      } else if ((flags & S_ZEROFILL)) {
         return ZerofillSection<bits>::Parse(img, offset);
      }

      throw error("bad section flags (section %s)", sect.sectname);
   }

   template <Bits bits>
   SymbolPointerSection<bits>::SymbolPointerSection(const Image& img, std::size_t offset):
      Section<bits>(img, offset) {
      pointers = std::vector<pointer_t>(&img.at<pointer_t>(this->sect.offset),
                                        &img.at<pointer_t>(this->sect.offset + this->sect.size));
   }

   template <Bits bits>
   const xed_state_t Instruction<bits>::dstate =
      {select_value(bits, XED_MACHINE_MODE_LEGACY_32, XED_MACHINE_MODE_LONG_64),
       select_value(bits, XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_32b)
      };

   template <Bits bits>
   Instruction<bits>::Instruction(const Image& img, std::size_t offset) {
      xed_decoded_inst_zero_set_mode(&xedd, &dstate);
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      xed_error_enum_t err;
      if ((err = xed_decode(&xedd, &img.at<uint8_t>(offset),
                            img.size() - offset
           )) != XED_ERROR_NONE) {
         throw error("%s: offset 0x%x: xed_decode: %s", __FUNCTION__, offset,
                     xed_error_enum_t2str(err));
      }

      instbuf = std::vector<uint8_t>(&img.at<uint8_t>(offset),
                                     &img.at<uint8_t>(offset + xed_decoded_inst_get_length(&xedd)));
   }

   template <Bits bits>
   TextSection<bits>::TextSection(const Image& img, std::size_t offset): Section<bits>(img, offset)
   {
      const std::size_t begin = this->sect.offset;
      const std::size_t end = begin + this->sect.size;
      std::size_t data = begin;
      for (std::size_t it = begin; it < end; ) {
         try {
            Instruction<bits> *ptr = Instruction<bits>::Parse(img, it);
            
            if (data != it) {
               content.push_back(DataBlob<bits>::Parse(img, data, it - data));
            }
            
            content.push_back(ptr);
            it += ptr->size();
            data = it;
         } catch (...) {
            ++it;
         }
      }

      if (data != end) {
         content.push_back(DataBlob<bits>::Parse(img, data, end - data));
      }
   }

   template <Bits bits>
   void TextSection<bits>::Parse2(const Image& img, Archive<bits>&& archive) {
      /* construct instruction vector */
      throw error("%s: stub", __FUNCTION__);
   }

   template <Bits bits>
   TextSection<bits>::~TextSection() {
      for (SectionBlob<bits> *ptr : content) {
         delete ptr;
      }
   }
   
   template class Section<Bits::M32>;
   template class Section<Bits::M64>;
   
}
