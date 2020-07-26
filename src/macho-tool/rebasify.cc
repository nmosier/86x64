#include <iostream>

#include "rebasify.hh"
#include "core/macho.hh"
#include "core/archive.hh"
#include "core/opcodes.hh"
#include "core/section_blob.hh"
#include "core/instruction.hh"
#include "core/rebase_info.hh"
#include "core/dyldinfo.hh"

int Rebasify::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;
      
   default: abort();
   }
}

void Rebasify::state_info::reset() {
   state = 0;
   vmaddr = 0;
   reg = XED_REG_EAX;
   frame_index = -1;
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

   state_info state(archive32);
   for (auto text_it = state.section->content.begin(); text_it != state.section->content.end();
        ++text_it) {
      auto text_inst = dynamic_cast<MachO::Instruction<MachO::Bits::M32> *>(*text_it);
      if (text_inst) {
         decode_info info(text_inst->xedd, text_it);
         if (handle_inst(text_inst, state, info) < 0) { return -1; }
      }
   }
      
   archive32->Build(0);
   archive32->Emit(*out_img);
   return 0;
}

Rebasify::decode_info::decode_info(const xed_decoded_inst_t& xedd,
                                   typename MachO::Section<MachO::Bits::M32>::Content::iterator it):
   text_it(it),
   reg0(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0)),
   reg1(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1)),
   iform(xed_decoded_inst_get_iform_enum(&xedd)),
   iclass(xed_decoded_inst_get_iclass(&xedd)),
   nmemops(xed_decoded_inst_number_of_memory_operands(&xedd)),
   base_reg(xed_decoded_inst_get_base_reg(&xedd, 0)),
   memdisp(xed_decoded_inst_get_memory_displacement(&xedd, 0)) {}

int Rebasify::handle_inst(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                           const decode_info& info) const {
   switch (state.state) {
   case 0: /* init */
      if (inst->instbuf == MachO::opcode_t({0xe8, 0x00, 0x00, 0x00, 0x00})) {
         state.state = 1;
      }
      return 0;
      
   case 1: /* seen call */
      if (inst->instbuf == MachO::opcode_t({0x58})) {
         state.state = 2;
         state.reg = XED_REG_EAX;
         state.vmaddr = inst->loc.vmaddr;
      }
      return 0;

   case 2:
      if (state.reg) {
         if ((info.iform == XED_IFORM_MOV_MEMv_OrAX || info.iform == XED_IFORM_MOV_MEMv_GPRv) &&
             info.reg0 == state.reg && info.base_reg == XED_REG_EBP) {
            /* frame store */
            state.frame_index = info.memdisp;
         } else {
            handle_inst_thunk(inst, state, info);
         }
      } else {
         if ((info.iform == XED_IFORM_MOV_GPRv_MEMv || info.iform == XED_IFORM_MOV_OrAX_MEMv) &&
             info.base_reg == XED_REG_EBP && info.memdisp == state.frame_index) {
            state.reg = info.reg0;
         }
      }
      return 0;

   default: abort();
   }
}

int Rebasify::handle_inst_thunk(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                                 const decode_info& info) const {   
   /* check if reads from or writes to eax */
   switch (xed_decoded_inst_get_iclass(&inst->xedd)) {
   case XED_ICLASS_RET_NEAR:
   case XED_ICLASS_RET_FAR:
      state.reset();
      state.state = 0;
      break;

   default:
      {
         std::size_t target = state.vmaddr;
         if (info.base_reg == state.reg) {
            target += info.memdisp;
         }
                  
         if (info.base_reg == state.reg || info.reg1 == state.reg) {
            auto mov_inst = new MachO::Instruction<MachO::Bits::M32>
               (MachO::opcode::mov_r32_imm32(*state.reg));
            auto mov_inst_imm = MachO::Immediate<MachO::Bits::M32>::Create(target);
            mov_inst->segment = mov_inst_imm->segment = state.segment;
            mov_inst->section = mov_inst_imm->section = state.section;

            if ((mov_inst_imm->pointee =
                 state.archive->template find_blob<MachO::SectionBlob>(target)) == nullptr) {
               log("unable to find destination blob in rebasify operation at vmaddr 0x%zx",
                   inst->loc.vmaddr);
               return -1;
            }
                     
            mov_inst->imm = mov_inst_imm;
            mov_inst->loc.vmaddr = (*info.text_it)->loc.vmaddr;

            state.section->content.insert(info.text_it, mov_inst);
                     
            /* adjust displacement if necessary */
            if (info.base_reg == state.reg) {
               const unsigned dispbits = xed_decoded_inst_get_memory_displacement_width_bits(&inst->xedd, 0);
               if (dispbits != 0) {
                  const xed_enc_displacement_t encdisp = {0, dispbits};
                  if (!xed_patch_disp(&inst->xedd, inst->instbuf.data(), encdisp)) {
                     log("error while patching displacement");
                     return -1;
                  }
               }
            }
                     
            /* add to rebase info */
            auto rebase_node = MachO::RebaseNode<MachO::Bits::M32>::
               Create(REBASE_TYPE_TEXT_ABSOLUTE32);
            rebase_node->blob = mov_inst_imm;
            auto dyld_info = state.archive->template subcommand<MachO::DyldInfo>();
            if (dyld_info == nullptr) {
               log("missing dyld info");
               return -1;
            }
            auto rebase_info = dyld_info->rebase;
            rebase_info->rebasees.push_back(rebase_node);
         }
         break;
      }
   }

   /* check if eax is destroyed */
   if (info.reg0 == state.reg) {
      switch (xed_decoded_inst_get_iclass(&inst->xedd)) {
      case XED_ICLASS_MOV:
         state.reg = std::nullopt;
         return 0;

      default:
         break;
      }
   }

   return 0;
}

Rebasify::state_info::state_info(MachO::Archive<MachO::Bits::M32> *archive):
   archive(archive), segment(archive->segment(SEG_TEXT)), section(archive->section(SECT_TEXT)) {
   reset();
}
