#include <unordered_map>

extern "C" {
#include <xed-interface.h>
}

#include "instruction.hh"
#include "image.hh"
#include "build.hh"
#include "parse.hh"
#include "transform.hh"
#include "opcodes.hh"
#include "section.hh"

namespace MachO {

   namespace {
      template <Bits bits>
      const xed_state_t dstate_ =
         {select_value(bits, XED_MACHINE_MODE_LEGACY_32, XED_MACHINE_MODE_LONG_64),
          select_value(bits, XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_32b)
         };

      typename SectionBlob<Bits::M32>::SectionBlobs push_r32(xed_reg_enum_t r32) {
         /* i386 | push r32
          * -----|---------
          * X86  | push ax
          *      | push ax
          *      | mov [rsp], r32
          * NOTE: Shouldn't modify flags.
          */
         auto push1 = new Instruction<Bits::M64>(opcode::push_ax());
         auto push2 = new Instruction<Bits::M64>(opcode::push_ax());
         auto mov = new Instruction<Bits::M64>(opcode::mov_mem_rsp_r32(r32));
         return {push1, push2, mov};
      }

      typename SectionBlob<Bits::M32>::SectionBlobs pop_r32(xed_reg_enum_t r32) {
         /* i386 | pop r32
          * -----|--------
          * X86  | mov r32,[rsp]
          *      | lea rsp,[rsp+4]
          * NOTE: Shouldn't modify flags.
          */
         auto mov = new Instruction<Bits::M64>(opcode::mov_r32_mem_rsp(r32));
         auto lea = new Instruction<Bits::M64>(opcode::lea_rsp_mem_rsp_4());
         return {mov, lea};
      }

      typename SectionBlob<Bits::M32>::SectionBlobs call_op(SectionBlob<Bits::M64> *jmp_inst) {
         /* i386 | call <op>
          * -----|----------
          * X86  | lea r11,[rip+<size>]
          *      | _push r11d
          *      | jmp <op>
          */
         auto lea_inst = new Instruction<Bits::M64>(opcode::lea_r11_mem_rip_disp32());
         auto push_insts = push_r32(XED_REG_R11D);
         auto ret_placeholder = Placeholder<Bits::M64>::Create();
         lea_inst->memidx = 0;
         lea_inst->memdisp = ret_placeholder;
         auto insts = push_insts;
         insts.push_front(lea_inst);
         insts.push_back(jmp_inst);
         insts.push_back(ret_placeholder);
         return insts;
      }
      
   }

   template <Bits bits>
   const xed_state_t& Instruction<bits>::dstate() { return dstate_<bits>; }
   
   template <Bits bits>
   Instruction<bits>::Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env):
      SectionBlob<bits>(loc, env), memdisp(nullptr), imm(nullptr), brdisp(nullptr)
   {
      xed_error_enum_t err;
      
      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
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



      /* Check for relocations */
      std::unordered_map<xed_iform_enum_t, std::size_t> reloc_index_map =
         {{XED_IFORM_CALL_NEAR_RELBRz, 1},
          {XED_IFORM_JMP_RELBRz, 1},
          {XED_IFORM_JZ_RELBRz, 2},
          {XED_IFORM_JNZ_RELBRz, 2},
          {XED_IFORM_JL_RELBRz, 2},
          {XED_IFORM_JLE_RELBRz, 2},
          {XED_IFORM_JNL_RELBRz, 2},
          {XED_IFORM_JNLE_RELBRz, 2},
          {XED_IFORM_JNBE_RELBRz, 2},
          {XED_IFORM_JBE_RELBRz, 2},
          {XED_IFORM_JB_RELBRz, 2},
          {XED_IFORM_JNB_RELBRz, 2},
          {XED_IFORM_LEA_GPRv_AGEN, 3},
          /* TODO -- probably missing a few */
         };

      auto reloc_index_map_it = reloc_index_map.find(xed_decoded_inst_get_iform_enum(&xedd));
      if (reloc_index_map_it != reloc_index_map.end()) {
         /* check if there is a relocation entry at this address */
         auto relocs_it = env.relocs.find(loc.vmaddr + reloc_index_map_it->second);
         if (relocs_it != env.relocs.end()) {
            reloc = relocs_it->second;
            env.relocs.erase(relocs_it);
            return;
         }
      }
      
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
                  this->memdisp = env.add_placeholder(targetaddr);
                  // env.vmaddr_resolver.resolve(targetaddr, &this->memdisp);

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
         this->brdisp = env.add_placeholder(targetaddr);
         // env.vmaddr_resolver.resolve(targetaddr, &this->brdisp);
      }

      /* Check for other immediates */
      switch (xed_decoded_inst_get_iform_enum(&xedd)) {
      case XED_IFORM_PUSH_IMMz: /* push imm32 */
      case XED_IFORM_MOV_GPRv_IMMv:
         assert(imm == nullptr);
         imm = Immediate<bits>::Parse(img, loc + (instbuf.size() - sizeof(uint32_t)), env, false);
         break;
      default:
         break;
      }

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
                        "vmaddr 0x%zx, iform %s\n", __FUNCTION__, this->loc.offset,
                        this->loc.vmaddr, xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xedd)));
         }
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

      if (imm) {
         imm->Emit(img, offset + instbuf.size() - imm->size());
      }
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
   typename SectionBlob<bits>::SectionBlobs Instruction<bits>::Transform(TransformEnv<bits>& env)
      const
   {
      assert(bits == Bits::M32);
      
      if (imm && imm->pointee) {
         assert(bits == Bits::M32);

         switch (xed_decoded_inst_get_iform_enum(&xedd)) {
         case XED_IFORM_PUSH_IMMz:
            {
               if (this->section->name() != SECT_SYMBOL_STUB &&
                   this->section->name() != SECT_STUB_HELPER) {
                  throw error("pushing abs32 in section `%s'", this->section->name().c_str());
               }
               
               /* i386 | push abs32
                * -----|---------------------
                * X86  | lea r11,[rip+disp32]
                *      | push r11
                */
               auto lea_inst = new Instruction<opposite<bits>>(opcode::lea_r11_mem_rip_disp32());
               lea_inst->memidx = 0; // TODO: is this right?
               env.resolve(imm->pointee, &lea_inst->memdisp);

               auto push_inst = new Instruction<opposite<bits>>(opcode::push_r11());

               return {lea_inst, push_inst};
            }

         case XED_IFORM_MOV_GPRv_IMMv:
            {
               /* i386 | mov r32, abs32
                * -----|---------------
                * X86  | lea r32, [rip+disp32]
                */
               const auto r32 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               auto lea_inst = new Instruction<opposite<bits>>(opcode::lea_r32_mem_rip_disp32(r32));
               lea_inst->memidx = 0; // TODO: is this right
               env.resolve(imm->pointee, &lea_inst->memdisp);
               return {lea_inst};
            }

         case XED_IFORM_JMP_MEMv:
            {
               if (this->section->name() != SECT_SYMBOL_STUB &&
                   this->section->name() != SECT_STUB_HELPER) {
                  throw error("%s:%d: stub\n", __FUNCTION__, __LINE__);
               }
               /* i386 | jmp [abs32]
                * -----|-----------------
                * X86  | jmp [rip+disp32]
                */  
               auto jmp_inst = new Instruction<opposite<bits>>(opcode::jmp_mem_rip_disp32());
               jmp_inst->memidx = 0; // TODO: is this right?
               env.resolve(imm->pointee, &jmp_inst->memdisp);
               
               return {jmp_inst};
            }

         default:
            throw error("%s: don't know how to translate i386 instruction with absolute memory " \
                        "addressing to x86_64 (iform = %s)", __FUNCTION__,
                        xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xedd)));
         }

      }

      if constexpr (bits == Bits::M32) {
            /* iform rules */
            switch (xed_decoded_inst_get_iform_enum(&xedd)) {
            case XED_IFORM_PUSH_GPRv_50: // push r32
               return push_r32(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0));

            case XED_IFORM_POP_GPRv_58:
               return pop_r32(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0));
               
            case XED_IFORM_CALL_NEAR_GPRv: // call r32
               {
                  auto jmp_inst = new Instruction<Bits::M64>
                     (opcode::jmp_r64(opcode::r32_to_r64(xed_decoded_inst_get_reg
                                                         (&xedd, XED_OPERAND_REG0))));
                  return call_op(jmp_inst);
               }

            case XED_IFORM_CALL_NEAR_MEMv:
               {
                  auto jmp = instbuf;
                  assert((jmp.at(1) & 0x30) == 0x20);
                  jmp.at(1) ^= 0x30;
                  auto jmp_inst = new Instruction<Bits::M64>(jmp);
                  env.resolve(brdisp, &jmp_inst->brdisp);
                  env.resolve(memdisp, &jmp_inst->memdisp);
                  return call_op(jmp_inst);
               }

            case XED_IFORM_CALL_NEAR_RELBRz:
               {
                  auto jmp_inst = new Instruction<Bits::M64>({0xe9, 0x00, 0x00, 0x00, 0x00});
                  env.resolve(memdisp, &jmp_inst->memdisp);
                  env.resolve(brdisp, &jmp_inst->brdisp);
                  return call_op(jmp_inst);
               }
               
            case XED_IFORM_CALL_NEAR_RELBRd:
               throw error("%s: don't know how to handle `XED_IFORM_CALL_NEAR_RELBRz' at vmaddr 0x%zx", __FUNCTION__, this->loc.vmaddr);
               
            case XED_IFORM_RET_NEAR:
               {
                  /* i386 | ret
                   * -----|-----
                   * X86  | _pop32 r11d
                   *      | jmp r11d
                   */
                  auto insts = pop_r32(XED_REG_R11D);
                  auto jmp_inst = new Instruction<opposite<bits>>(opcode::jmp_r64(XED_REG_R11));
                  insts.push_back(jmp_inst);
                  return insts;
               }
               
            default: break;
            }

            /* default rule */
            return {new Instruction<opposite<bits>>(*this, env)};
            
         } else {
         abort();
      }
      
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

      assert(!(other.imm && other.imm->pointee));
      
      if (other.imm) {
         imm = other.imm->Transform_one(env);
      }
      
      if (other.brdisp) {
         env.resolve(other.brdisp, &brdisp);
      }

      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      xed_error_enum_t err;
      if ((err = xed_decode(&xedd, instbuf.data(), instbuf.size())) != XED_ERROR_NONE) {
         throw error("%s: xed_decode: %s\n", __FUNCTION__, xed_error_enum_t2str(err));
      }
   }

   template <Bits bits>
   void Instruction<bits>::decode() {
      xed_error_enum_t err;
      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
      if ((err = xed_decode(&xedd, instbuf.data(), instbuf.size())) != XED_ERROR_NONE) {
         throw error("%s: xed_decode: %s\n", __FUNCTION__, xed_error_enum_t2str(err));
      }
   }


   template class Instruction<Bits::M32>;
   template class Instruction<Bits::M64>;   
   
}
