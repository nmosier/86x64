#include <iostream>

#include "rebasify.hh"
#include "macho.hh"
#include "archive.hh"
#include "opcodes.hh"
#include "section_blob.hh"
#include "instruction.hh"
#include "rebase_info.hh"
#include "dyldinfo.hh"

int Rebasify::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;
      
   default: abort();
   }
}

int Rebasify::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
   auto archive32 = dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho);
   if (archive32 == nullptr) {
      log("input Mach-O not a 32-bit archive");
      return -1;
   }

   /* ALGORITHM
    * - Pattern-match for 'call 0 \ pop eax'
    * - Remove pattern.
    * - Find all subsequent uses of %eax (before any conditional branches?)
    * ...
    */
   // TODO
   auto text = archive32->section(SECT_TEXT);
   if (text == nullptr) {
      log("missing text section");
      return -1;
   }

   int state = 0;
   std::size_t eax_vmaddr;
   const MachO::opcode_t call_opcode = {0xe8, 0x00, 0x00, 0x00, 0x00};
   const MachO::opcode_t pop_opcode = {0x58};
   for (auto text_it = text->content.begin(); text_it != text->content.end(); ++text_it) {
      auto inst = dynamic_cast<MachO::Instruction<MachO::Bits::M32> *>(*text_it);
      if (inst == nullptr) { continue; }
      if (state < 2) {
         if (inst->instbuf == call_opcode) {
            state = 1;
         } else if (inst->instbuf == pop_opcode && state == 1) {
            state = 2;
            eax_vmaddr = inst->loc.vmaddr;
            fprintf(stderr, "[REBASIFY] pop, eax = 0x%zx\n", eax_vmaddr);
         } else {
            state = 0;
         }
      } else {
         /* check if reads from or writes to eax */
         const xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(&inst->xedd, XED_OPERAND_REG0);
         const xed_reg_enum_t reg1 = xed_decoded_inst_get_reg(&inst->xedd, XED_OPERAND_REG1);
         const unsigned nmemops = xed_decoded_inst_number_of_memory_operands(&inst->xedd);

         switch (xed_decoded_inst_get_iclass(&inst->xedd)) {
         case XED_ICLASS_PUSH: /* give up on tracking %eax */
         case XED_ICLASS_POP: /* %eax overwritten */
         case XED_ICLASS_CALL_FAR:
         case XED_ICLASS_CALL_NEAR:
         case XED_ICLASS_JMP:
         case XED_ICLASS_RET_NEAR:
         case XED_ICLASS_RET_FAR:
         case XED_ICLASS_JB:
         case XED_ICLASS_JBE: 
         case XED_ICLASS_JCXZ: 
         case XED_ICLASS_JECXZ: 
         case XED_ICLASS_JL: 
         case XED_ICLASS_JLE:
         case XED_ICLASS_JMP_FAR: 
         case XED_ICLASS_JNB: 
         case XED_ICLASS_JNBE: 
         case XED_ICLASS_JNL: 
         case XED_ICLASS_JNLE: 
         case XED_ICLASS_JNO: 
         case XED_ICLASS_JNP: 
         case XED_ICLASS_JNS: 
         case XED_ICLASS_JNZ: 
         case XED_ICLASS_JO: 
         case XED_ICLASS_JP: 
         case XED_ICLASS_JRCXZ: 
         case XED_ICLASS_JS: 
         case XED_ICLASS_JZ: 
            state = 0;
            break;

#if 0
         case XED_ICLASS_LEA:
            {
               const int64_t disp = xed_decoded_inst_get_memory_displacement(&inst->xedd, 0);
               if (reg0 == XED_REG_EAX &&
                   xed_decoded_inst_get_base_reg(&inst->xedd, 0) == XED_REG_EAX) {
                  /* update value of eax_vmaddr */
                  eax_vmaddr += disp;
               } else if (reg0 == XED_REG_EAX) {
                  state = 0; /* eax is overwritten */
               } else if (reg1 == XED_REG_EAX) {
                  /* eax used in address expression */
                  const std::size_t vmaddr = eax_vmaddr + disp;

                  /* find target */
                  if ((memdisp = archive->template find_blob<SectionBlob>(vmaddr)) == nullptr) {
                     fprintf(stderr, "warning: unable to find memdisp for thunk eax\n");
                  }
                  
                  
#warning ADD REBASE
               }
               fprintf(stderr, "[REBASIFY] 0x%zx, lea = 0x%zx\n", inst->loc.vmaddr, eax_vmaddr);
            }
            break;
#endif
               
         default:
            {
               std::size_t target = eax_vmaddr;
                  
               /* use index */
               unsigned memidx;
               for (memidx = 0; memidx < nmemops; ++memidx) {
                  if (xed_decoded_inst_get_base_reg(&inst->xedd, memidx) == XED_REG_EAX) {
                     const int64_t disp =
                        xed_decoded_inst_get_memory_displacement(&inst->xedd, memidx);
                     target += disp;
                     break;
                  }
               }

               if (memidx != nmemops || reg1 == XED_REG_EAX) {
                  auto mov_inst = new MachO::Instruction<MachO::Bits::M32>(MachO::opcode::mov_eax_imm32());
                  auto mov_inst_imm = MachO::Immediate<MachO::Bits::M32>::Create(target);
                  if ((mov_inst->segment = mov_inst_imm->segment = archive32->segment(SEG_TEXT)) == nullptr) {
                     log("missing TEXT segment");
                     return -1;
                  }
                  if ((mov_inst->section = mov_inst_imm->section = archive32->section(SECT_TEXT)) == nullptr) {
                     log("missing text section");
                     return -1;
                  }
                  if ((mov_inst_imm->pointee = archive32->find_blob<MachO::SectionBlob>(target)) == nullptr) {
                     log("unable to find destination blob in rebasify operation at vmaddr 0x%zx",
                         inst->loc.vmaddr);
                     return -1;
                  }

                  mov_inst->imm = mov_inst_imm;

                  text->content.insert(text_it, mov_inst);

                  /* adjust displacement if necessary */
                  if (memidx < nmemops) {
                     const xed_enc_displacement_t encdisp =
                        {0, xed_decoded_inst_get_memory_displacement_width_bits(&inst->xedd, memidx)};
                     if (!xed_patch_disp(&inst->xedd, inst->instbuf.data(), encdisp)) {
                        log("error while patching displacement");
                        return -1;
                     }
                  }

                  /* add to rebase info */
                  auto rebase_node = MachO::RebaseNode<MachO::Bits::M32>::Create(REBASE_TYPE_TEXT_ABSOLUTE32);
                  rebase_node->blob = mov_inst_imm;
                  auto dyld_info = archive32->template subcommand<MachO::DyldInfo>();
                  if (dyld_info == nullptr) {
                     log("missing dyld info");
                     return -1;
                  }
                  auto rebase_info = dyld_info->rebase;
                  rebase_info->rebasees.push_back(rebase_node);
                  
                  
                  fprintf(stderr, "[REBASIFY] 0x%zx, target = 0x%zx\n", inst->loc.vmaddr, target);
               }
               break;
            }
         }
      }
   }
      
   archive32->Build(0);
   archive32->Emit(*out_img);
   return 0;
}
