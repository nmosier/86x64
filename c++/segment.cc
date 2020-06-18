#include "segment.hh"

namespace MachO {

   template <Bits bits>
   Segment<bits>::Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      segment_command = img.at<segment_command_t>(offset);

      offset += sizeof(segment_command_t);
      for (int i = 0; i < segment_command.nsects; ++i) {
         Section<bits> *section = Section<bits>::Parse(img, offset, env);
         sections.push_back(section);
         offset += section->size();
      }
   }

   template <Bits bits>
   Segment<bits>::~Segment() {
      for (Section<bits> *ptr : sections) {
         delete ptr;
      }
   }   

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      section_t sect = img.at<section_t>(offset);
      uint32_t flags = sect.flags;
      
      if ((flags & S_ATTR_PURE_INSTRUCTIONS)) {
         return TextSection<bits>::Parse(img, offset, env);
      } else if ((flags & S_ATTR_SOME_INSTRUCTIONS)) {
         throw error("segments with only 'some' instructions not supported");
      } else if (flags == S_NON_LAZY_SYMBOL_POINTERS || flags == S_LAZY_SYMBOL_POINTERS) {
         return SymbolPointerSection<bits>::Parse(img, offset, env);
      } else if (flags == S_REGULAR || flags == S_CSTRING_LITERALS) {
         return DataSection<bits>::Parse(img, offset, env);
      } else if ((flags & S_ZEROFILL)) {
         return ZerofillSection<bits>::Parse(img, offset, env);
      }

      throw error("bad section flags (section %s)", sect.sectname);
   }

   template <Bits bits>
   const xed_state_t Instruction<bits>::dstate =
      {select_value(bits, XED_MACHINE_MODE_LEGACY_32, XED_MACHINE_MODE_LONG_64),
       select_value(bits, XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_32b)
      };

   template <Bits bits>
   Instruction<bits>::Instruction(const Image& img, std::size_t offset, std::size_t vmaddr,
                                  ParseEnv<bits>& env):
      SectionBlob<bits>(vmaddr, env), memdisp(nullptr), brdisp(nullptr)
   {
      xed_decoded_inst_t xedd;
      xed_error_enum_t err;
      
      xed_decoded_inst_zero_set_mode(&xedd, &dstate);
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      if ((err = xed_decode(&xedd, &img.at<uint8_t>(offset), img.size() - offset)) !=
          XED_ERROR_NONE) {
         throw error("%s: offset 0x%x: xed_decode: %s", __FUNCTION__, offset,
                     xed_error_enum_t2str(err));
      }
      instbuf = std::vector<uint8_t>(&img.at<uint8_t>(offset),
                                     &img.at<uint8_t>(offset + xed_decoded_inst_get_length(&xedd)));
      const std::size_t refaddr = vmaddr + xed_decoded_inst_get_length(&xedd);
      
      

      xed_operand_values_t *operands = xed_decoded_inst_operands(&xedd);
      unsigned int memops = xed_operand_values_number_of_memory_operands(operands);
      if (xed_operand_values_has_memory_displacement(operands)) {
         for (unsigned i = 0; i < memops; ++i) {
            xed_reg_enum_t basereg  = xed_decoded_inst_get_base_reg(operands,  i);
            xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(operands, i);
            if (basereg == select_value(bits, XED_REG_EIP, XED_REG_RIP) &&
                indexreg == XED_REG_INVALID)
               {
                  if (memdisp) {
                     fprintf(stderr,
                             "warning: %s: duplicate memdisp at offset 0x%jx, vmaddr 0x%jx\n",
                             __FUNCTION__, offset, vmaddr);
                     abort();
                  }
                  
                  /* get memory displacement & reference address */
                  const ssize_t memdisp = xed_decoded_inst_get_memory_displacement(operands,
                                                                                        i);
                  const std::size_t targetaddr = refaddr + memdisp;

                  /* resolve pointer */
                  env.resolve(targetaddr, &this->memdisp);
               }
         }
      }

      /* Relative Branches */
      if (xed_operand_values_has_branch_displacement(operands)) {
         const ssize_t brdisp = xed_decoded_inst_get_branch_displacement(operands);
         const size_t targetaddr = refaddr + brdisp;

         /* resolve */
         env.resolve(targetaddr, &this->brdisp);
      }

   }

   template <Bits bits>
   TextSection<bits>::TextSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      Section<bits>(img, offset)
   {
      const std::size_t begin = this->sect.offset;
      const std::size_t end = begin + this->sect.size;
      for (std::size_t it = begin; it < end; ) {
         try {
            const std::size_t vmaddr = this->sect.addr + (it - begin);
            Instruction<bits> *ptr = Instruction<bits>::Parse(img, it, vmaddr, env);
            content.push_back(ptr);
            it += ptr->size();
         } catch (...) {
            content.push_back(DataBlob<bits>::Parse(img, it, this->sect.addr + (it - begin),
                                                    env));
            ++it;
         }
      }
   }

   template <Bits bits>
   TextSection<bits>::~TextSection() {
      for (SectionBlob<bits> *ptr : content) {
         delete ptr;
      }
   }
   
   template <Bits bits>
   std::size_t Segment<bits>::size() const {
      return sizeof(segment_command) + sections.size() * Section<bits>::size();
   }

   template <Bits bits>
   SectionBlob<bits>::SectionBlob(std::size_t vmaddr, ParseEnv<bits>& env) {
      env.add(vmaddr, this);
   }

   template <Bits bits>
   DataSection<bits>::DataSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      Section<bits>(img, offset)
   {
      const std::size_t begin = this->sect.offset;
      const std::size_t end = begin + this->sect.size;
      for (std::size_t it = begin, vmaddr = this->sect.addr; it != end; ++it, ++vmaddr) {
         content.push_back(DataBlob<bits>::Parse(img, offset, vmaddr, env));
      }
   }

   // OPTIMIZE
   template <Bits bits>
   ZerofillSection<bits>::ZerofillSection(const Image& img, std::size_t offset,
                                          ParseEnv<bits>& env): Section<bits>(img, offset) {
      const std::size_t begin = this->sect.offset;
      const std::size_t end = begin + this->sect.size;
      for (std::size_t it = begin, vmaddr = this->sect.addr; it != end; ++it, ++vmaddr) {
         content.push_back(ZeroBlob<bits>::Parse(vmaddr, env));
      }
   }

   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const Image& img, std::size_t offset,
                                              std::size_t vmaddr, ParseEnv<bits>& env):
      SymbolPointer<bits>(vmaddr, env) {
      // STUB
      throw error("%s: stub", __FUNCTION__);
   }
      
   template class Segment<Bits::M32>;
   template class Segment<Bits::M64>;

}
