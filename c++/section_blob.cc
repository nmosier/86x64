#include "section_blob.hh"
#include "image.hh"
#include "build.hh"
#include "parse.hh"
#include "transform.hh"

namespace MachO {

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

#if 0
                  char pbuf[32];
                  xed_print_info_t pinfo;
                  xed_init_print_info(&pinfo);
                  pinfo.blen = 32;
                  pinfo.buf = pbuf;
                  pinfo.p = &xedd;
                  pinfo.runtime_address = loc.vmaddr;
                  xed_format_generic(&pinfo);
                  fprintf(stderr, "%s\n", pbuf);
#endif
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
   SectionBlob<bits>::SectionBlob(const Location& loc, ParseEnv<bits>& env):
      segment(env.current_segment), loc(loc)
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

   template <Bits bits>
   void SectionBlob<bits>::Build(BuildEnv<bits>& env) {
      env.allocate(size(), loc);
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

   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const LazySymbolPointer<opposite<bits>>& other,
                                              TransformEnv<opposite<bits>>& env):
      SymbolPointer<bits>(other, env), pointee(nullptr)
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

   template <Bits bits>
   DataBlob<bits>::DataBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
      SectionBlob<bits>(loc, env), data(img.at<uint8_t>(loc.offset)) {}

   template <Bits bits>
   DataBlob<bits>::DataBlob(const DataBlob<opposite<bits>>& other,
                            TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), data(other.data) {}

   template class SectionBlob<Bits::M32>;
   template class SectionBlob<Bits::M64>;
   
   template class Instruction<Bits::M32>;
   template class Instruction<Bits::M64>;
   
   template class LazySymbolPointer<Bits::M32>;
   template class LazySymbolPointer<Bits::M64>;

   template class DataBlob<Bits::M32>;
   template class DataBlob<Bits::M64>;
}
