#include <string>

#include "section.hh"

namespace MachO {

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      section_t<bits> sect = img.at<section_t<bits>>(offset);
      uint32_t flags = sect.flags;

      /* chcek whether contains instructions */
      std::vector<std::string> text_sectnames = {SECT_TEXT, SECT_STUBS, SECT_STUB_HELPER,
                                                 SECT_SYMBOL_STUBS};
      for (const std::string& sectname : text_sectnames) {
         if (sectname == sect.sectname) {
            return TextSection<bits>::Parse(img, offset, env);
         }
      }
      
      if (flags == S_LAZY_SYMBOL_POINTERS) {
         return LazySymbolPointerSection<bits>::Parse(img, offset, env);
      } else if (flags == S_NON_LAZY_SYMBOL_POINTERS) {
         return NonLazySymbolPointerSection<bits>::Parse(img, offset, env);
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
   Instruction<bits>::Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env):
      SectionBlob<bits>(loc, env), memdisp(nullptr), imm(nullptr), brdisp(nullptr)
   {
      xed_error_enum_t err;
      
      xed_decoded_inst_zero_set_mode(&xedd, &dstate);
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      if ((err = xed_decode(&xedd, &img.at<uint8_t>(loc.offset), img.size() - loc.offset)) !=
          XED_ERROR_NONE) {
         throw error("%s: offset 0x%x: xed_decode: %s", __FUNCTION__, loc.offset,
                     xed_error_enum_t2str(err));
      }
      instbuf = std::vector<uint8_t>(&img.at<uint8_t>(loc.offset),
                                     &img.at<uint8_t>(loc.offset + xed_decoded_inst_get_length(&xedd)));
      const std::size_t refaddr = loc.vmaddr + xed_decoded_inst_get_length(&xedd);
      
      

      xed_operand_values_t *operands = xed_decoded_inst_operands(&xedd);

      /* Memory Accesses */
      const unsigned int nops = xed_decoded_inst_noperands(&xedd);
      if (xed_operand_values_has_memory_displacement(operands)) {
         for (unsigned i = 0; i < nops; ++i) {
            xed_reg_enum_t basereg  = xed_decoded_inst_get_base_reg(operands,  i);
            xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(operands, i);
            if (basereg == select_value(bits, XED_REG_EIP, XED_REG_RIP) &&
                indexreg == XED_REG_INVALID)
               {
                  if (memdisp) {
                     fprintf(stderr,
                             "warning: %s: duplicate memdisp at offset 0x%jx, vmaddr 0x%jx\n",
                             __FUNCTION__, loc.offset, loc.vmaddr);
                     abort();
                  }

                  memidx = i;
                  
                  /* get memory displacement & reference address */
                  const ssize_t memdisp = xed_decoded_inst_get_memory_displacement(operands, i);
                  const std::size_t targetaddr = refaddr + memdisp;
                  
                  /* resolve pointer */
                  env.vmaddr_resolver.resolve(targetaddr, &this->memdisp);
                  
                  char pbuf[32];
                  xed_print_info_t pinfo;
                  xed_init_print_info(&pinfo);
                  pinfo.blen = 32;
                  pinfo.buf = pbuf;
                  pinfo.p = &xedd;
                  pinfo.runtime_address = loc.vmaddr;
                  xed_format_generic(&pinfo);
                  fprintf(stderr, "%s\n", pbuf);
               } else if (basereg == XED_REG_INVALID && indexreg == XED_REG_INVALID &&
                          xed_decoded_inst_get_memory_displacement_width(operands, i) ==
                          sizeof(uint32_t)) {
               /* simple pointer */
               const std::size_t idx = instbuf.size() - sizeof(uint32_t);
               imm = Immediate<bits>::Parse(img, loc + idx, env, true);
            }
         }
      }
      
      /* Relative Branches */
      if (xed_operand_values_has_branch_displacement(operands)) {
         const ssize_t brdisp = xed_decoded_inst_get_branch_displacement(operands);
         const size_t targetaddr = refaddr + brdisp;

         /* resolve */
         env.vmaddr_resolver.resolve(targetaddr, &this->brdisp);
      }

      /* Check for other immediates */
      switch (xed_decoded_inst_get_iform_enum(&xedd)) {
      case XED_IFORM_PUSH_IMMz: /* push imm32 */
         imm = Immediate<bits>::Parse(img, loc + (instbuf.size() - sizeof(uint32_t)), env, false);
         break;
      default:
         break;
      }
      
   }

   template <Bits bits>
   Immediate<bits>::Immediate(const Image& img, const Location& loc, ParseEnv<bits>& env,
                              bool is_pointer):
      SectionBlob<bits>(loc, env), value(img.at<uint32_t>(loc.offset)), pointee(nullptr) 
   {
      if (is_pointer) {
         env.vmaddr_resolver.resolve(loc.vmaddr, &pointee);
      }
   }
   
   template <Bits bits>
   SectionBlob<bits> *TextSection<bits>::BlobParser(const Image& img, const Location& loc,
                                                    ParseEnv<bits>& env) {
      try {
         return Instruction<bits>::Parse(img, loc, env);
      } catch (...) {
         return DataBlob<bits>::Parse(img, loc, env);
      }
   }

   template <Bits bits>
   SectionBlob<bits>::SectionBlob(const Location& loc, ParseEnv<bits>& env):
      segment(env.current_segment)
   {
      assert(segment);
      env.vmaddr_resolver.add(loc.vmaddr, this);
      env.offset_resolver.add(loc.offset, this);
   }

   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const Image& img, const Location& loc,
                                              ParseEnv<bits>& env):
      SymbolPointer<bits>(loc, env)
   {
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      
      std::size_t targetaddr = img.at<ptr_t>(loc.offset);
      env.vmaddr_resolver.resolve(targetaddr, (const SectionBlob<bits> **) &pointee);
   }

   template <Bits bits, template <Bits> typename Elem>
   SectionT<bits, Elem>::~SectionT() {
      for (Elem<bits> *elem : content) {
         delete elem;
      }
   }

   template <Bits bits, template <Bits> typename Elem>
   SectionT<bits, Elem>::SectionT(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                                  Parser parser): Section<bits>(img, offset) {
      const std::size_t begin = this->sect.offset;
      const std::size_t end = begin + this->sect.size;
      std::size_t it = begin;
      std::size_t vmaddr = this->sect.addr;
      while (it != end) {
         Elem<bits> *elem = parser(img, Location(it, vmaddr), env);
         content.push_back(elem);
         it += elem->size();
         vmaddr += elem->size();
      }
   }

   template <Bits bits>
   std::string Section<bits>::name() const {
      return std::string(sect.sectname, strnlen(sect.sectname, sizeof(sect.sectname)));
   }

   template <Bits bits>
   void SectionBlob<bits>::Build(BuildEnv<bits>& env) {
      env.allocate(size(), loc);
   }

   template <Bits bits>
   void Section<bits>::Build(BuildEnv<bits>& env) {
      env.align(sect.align);
      loc(env.loc);
      Build_content(env);
      sect.size = env.loc.offset - loc().offset;
      fprintf(stderr, "[BUILD] segment %s, section %s @ offset 0x%zx, vmaddr 0x%zx\n", sect.segname,
              sect.sectname, loc().offset, loc().vmaddr);
   }

   template <Bits bits>
   void ZerofillSection<bits>::Build(BuildEnv<bits>& env) {
      // env.align(this->sect.align);
      this->sect.size = this->content_size();
      this->loc(Location(0, env.loc.vmaddr));
   }

   template <Bits bits, template <Bits> typename Elem>
   void SectionT<bits, Elem>::Build_content(BuildEnv<bits>& env) {
      for (Elem<bits> *elem : content) {
         elem->Build(env);
      }
   }
   
   template <Bits bits, template <Bits> typename Elem>
   std::size_t SectionT<bits, Elem>::content_size() const {
      std::size_t size = 0;
      for (const Elem<bits> *elem : content) {
         size += elem->size();
      }
      return align<bits>(size);
   }

   template <Bits bits>
   void Section<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<section_t<bits>>(offset) = sect;
      Emit_content(img, sect.offset);
   }
   
   template <Bits bits, template <Bits> typename Elem>
   void SectionT<bits, Elem>::Emit_content(Image& img, std::size_t offset) const {
      for (const Elem<bits> *elem : content) {
         elem->Emit(img, offset);
         offset += elem->size();
      }
   }

   template <Bits bits>
   void DataBlob<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<uint8_t>(offset) = data;
   }

   template <Bits bits>
   void SymbolPointer<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<ptr_t>(offset) = raw_data();
   }

   template <Bits bits>
   void Instruction<bits>::Emit(Image& img, std::size_t offset) const {
      /* patch instruction */
      xed_decoded_inst_t xedd = this->xedd;
      std::vector<uint8_t> instbuf = this->instbuf;
      
      if (memdisp) {
         const unsigned width_bits =
            xed_decoded_inst_get_memory_displacement_width_bits(&xedd, memidx);
         const std::size_t disp = memdisp->loc.vmaddr -
            (ssize_t) (this->loc.vmaddr + size());
         xed_enc_displacement_t enc; // = {disp, width_bits};
         enc.displacement = disp;
         enc.displacement_bits = width_bits;
         if (!xed_patch_disp(&xedd, &*instbuf.begin(), enc)) {
            throw error("%s: xed_patch_disp: failed to patch instruction at offset 0x%zx, " \
                        "vmaddr 0x%zx\n", __FUNCTION__, this->loc.offset, this->loc.vmaddr);
         }
      }

      if (imm) {
         imm->Emit(img, offset + instbuf.size() - imm->size());
      }
      
      if (brdisp) {
         const unsigned width_bits = xed_decoded_inst_get_branch_displacement_width_bits(&xedd);
         const std::size_t disp = brdisp->loc.vmaddr - (ssize_t) (this->loc.vmaddr + size());
         xed_encoder_operand_t enc;
         enc.u.brdisp = disp;
         enc.width_bits = width_bits;
         if (!xed_patch_relbr(&xedd, &*instbuf.begin(), enc)) {
            throw error("%s: xed_patch_relbr: failed to patch instruction at offset 0x%zx, " \
                        "vmaddr 0x%zx\n", __FUNCTION__, this->loc.offset, this->loc.vmaddr);
         }
      }

      /* emit instruction bytes */
      img.copy(offset, &*instbuf.begin(), instbuf.size());
   }

   template <Bits bits>
   void Instruction<bits>::Build(BuildEnv<bits>& env) {
      SectionBlob<bits>::Build(env);
      if (imm) {
         BuildEnv immenv(env.archive, env.loc - imm->size());
         imm->Build(immenv);
      }
   }

   template <Bits bits, template <Bits> typename Elem>
   void SectionT<bits, Elem>::Insert(const SectionLocation<bits>& loc, SectionBlob<bits> *blob) {
      Elem<bits> *elem = dynamic_cast<Elem<bits> *>(blob);
      if (elem == nullptr) {
         throw std::invalid_argument(std::string(__FUNCTION__) + ": blob is of incorrect type");
      }
      content.insert(content.begin() + loc.index, elem);
   }

   template <Bits bits>
   SectionBlob<bits>::SectionBlob(const SectionBlob<opposite<bits>>& other,
                                  TransformEnv<opposite<bits>>& env): segment(nullptr) {
      env.add(&other, this);
      env.resolve(other.segment, &segment);
   }

   template <Bits bits>
   Instruction<bits>::Instruction(const Instruction<opposite<bits>>& other,
                                  TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), instbuf(other.instbuf), memidx(other.memidx),
      memdisp(nullptr), imm(nullptr), brdisp(nullptr)
   {
      if (other.memdisp) {
         env.resolve(other.memdisp, &memdisp);
      }
      
      if (other.imm) {
         imm = other.imm->Transform(env);
      }
      
      if (other.brdisp) {
         env.resolve(other.brdisp, &brdisp);
      }

      xed_decoded_inst_zero_set_mode(&xedd, &dstate);
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      xed_error_enum_t err;
      if ((err = xed_decode(&xedd, instbuf.data(), instbuf.size())) != XED_ERROR_NONE) {
         throw error("%s: xed_decode: %s\n", __FUNCTION__, xed_error_enum_t2str(err));
      }

      if (xed_decoded_inst_get_length(&xedd) != xed_decoded_inst_get_length(&other.xedd)) {
         throw error("%s: lengths of transformed instruction mismatch\n", __FUNCTION__);
      }
   }

   template <Bits bits, template <Bits> typename Elem>
   SectionT<bits, Elem>::SectionT(const SectionT<opposite<bits>, Elem>& other,
                                  TransformEnv<opposite<bits>>& env): Section<bits>(other, env)
   {
      for (const auto elem : other.content) {
         content.push_back(elem->Transform(env));
      }
   }

   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const LazySymbolPointer<opposite<bits>>& other,
                                              TransformEnv<opposite<bits>>& env):
      SymbolPointer<bits>(other, env), pointee(nullptr)
   {
      env.resolve(other.pointee, &pointee);
   }

   template <Bits bits>
   InstructionPointer<bits>::InstructionPointer(const Image& img, const Location& loc,
                                                ParseEnv<bits>& env): SectionBlob<bits>(loc, env),
                                                                      pointee(nullptr) {
      const std::size_t vmaddr = img.at<uint32_t>(loc.offset);
      env.vmaddr_resolver.resolve(vmaddr, &pointee);
   }

   template <Bits bits>
   void InstructionPointer<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<uint32_t>(offset) = pointee->loc.vmaddr;
   }

   template <Bits bits>
   InstructionPointer<bits>::InstructionPointer(const InstructionPointer<opposite<bits>>& other,
                                                TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), pointee(nullptr)
   {
      env.resolve(other.pointee, &pointee);
   }

   template <Bits bits>
   Immediate<bits>::Immediate(const Immediate<opposite<bits>>& other,
                              TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), value(other.value), pointee(nullptr)
   {
      env.resolve(other.pointee, &pointee);
   }

   template <Bits bits>
   void Immediate<bits>::Emit(Image& img, std::size_t offset) const {
      uint32_t value;
      if (pointee) {
         value = pointee->loc.vmaddr;
      } else {
         value = this->value;
      }
      img.at<uint32_t>(offset) = value;
   }
   
   template class Section<Bits::M32>;
   template class Section<Bits::M64>;
   
}
