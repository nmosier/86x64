#include <iostream>
#include <list>

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

   case 'v':
      verbose = true;
      return 1;
      
   default: abort();
   }
}

void Rebasify::state_info::reset() {
   state = 0;
   vmaddr = 0;
   live_regs.clear();
   frame_index = std::nullopt;
   // text_it = section->content.begin();
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

   #if 0
   state_info state(archive32);
   for (auto text_it = state.section->content.begin(); text_it != state.section->content.end();
        ++text_it) {
      auto text_inst = dynamic_cast<MachO::Instruction<MachO::Bits::M32> *>(*text_it);
      if (text_inst) {
         decode_info info(text_inst->xedd, text_it);
         if (handle_inst(text_inst, state, info) < 0) { return -1; }
      }
   }
   #else
   handle_insts(archive32);
   #endif
      
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

void Rebasify::handle_insts(MachO::Archive<MachO::Bits::M32> *archive) const {
   std::list<state_info> states;
   std::unordered_set<size_t> visited_vmaddrs;

   /* initial state */
   states.emplace_front(archive);

   while (!states.empty()) {
      state_info state = states.front();
      states.pop_front();

      /* check if state has already been visited or has reached end of section */
      if (state.text_it == state.section->content.end() ||
          visited_vmaddrs.find((**state.text_it).loc.vmaddr) != visited_vmaddrs.end()) {
         continue;
      }

      if (verbose) {
         fprintf(stderr, "[REBASIFY] 0x%zx\n", (**state.text_it).loc.vmaddr);
      }

      MachO::Instruction<MachO::Bits::M32> *inst =
         dynamic_cast<MachO::Instruction<MachO::Bits::M32> *>(*state.text_it);
      
      if (inst) {
         /* add to visited list */
         visited_vmaddrs.insert(inst->loc.vmaddr);
         
         decode_info decode(inst->xedd, state.text_it);
         
         
         switch (decode.iclass) {
         case XED_ICLASS_JB: 
         case XED_ICLASS_JBE: 
         case XED_ICLASS_JL: 
         case XED_ICLASS_JLE: 
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
         case XED_ICLASS_JS: 
         case XED_ICLASS_JZ:
            /* split into two states */
            states.emplace_front(state, inst->brdisp);
            ++state.text_it;
            break;

         case XED_ICLASS_JMP:
            {
               /* add next instruction to be processed last in case it's not reachable thru static
                * analysis */
               state_info state2 = state;
               ++state2.text_it;
               states.push_back(state2);

               /* move state */
               state = state_info(state, inst->brdisp);
            }
            break;

         default:
            handle_inst(inst, state, decode);
            ++state.text_it;
            break;
         }

      } else {
         ++state.text_it;
      }

      states.push_front(state);
   }
}

int Rebasify::handle_inst(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                           const decode_info& info) const {
   const MachO::opcode_t call_0({0xe8, 0x00, 0x00, 0x00, 0x00});
   // const MachO::opcode_t pop({0x58});

   if (inst->instbuf == call_0) {
      if (verbose) {
         fprintf(stderr, "[REBASIFY] 0x%zx call 0x0\n", inst->loc.vmaddr);
      }
      state.state = 1;
      return 0;
   }
   
   switch (state.state) {
   case 0: /* init */
      return 0;
      
   case 1: /* seen call */
      if (info.iform == XED_IFORM_POP_GPRv_58) {
         state.state = 2;
         xed_reg_enum_t live_reg = xed_decoded_inst_get_reg(&inst->xedd, XED_OPERAND_REG0);
         state.live_regs.insert(live_reg);
         state.vmaddr = inst->loc.vmaddr;
         if (verbose) {
            fprintf(stderr, "[REBASIFY] 0x%zx thunk assigned to register %s\n",
                    inst->loc.vmaddr, xed_reg_enum_t2str(live_reg));
         }
      }
      return 0;

   case 2:
      if (info.iclass == XED_ICLASS_CALL_NEAR) {
         for (auto live_reg_it = state.live_regs.begin(); live_reg_it != state.live_regs.end(); ) {
            /* check if it's a preserved register */
            switch (*live_reg_it) {
            case XED_REG_EAX:
            case XED_REG_ECX:
            case XED_REG_EDX:
               if (verbose) {
                  fprintf(stderr, "[REBASIFY] 0x%zx register %s destroyed by call\n",
                          inst->loc.vmaddr, xed_reg_enum_t2str(*live_reg_it));
               }
               live_reg_it = state.live_regs.erase(live_reg_it);
               break;
            case XED_REG_EBX:
            case XED_REG_EDI:
            case XED_REG_ESI:
               ++live_reg_it;
               break;
            default: abort();
            }
         }
      } else {
         /* look for frame store */
         if ((info.iform == XED_IFORM_MOV_MEMv_OrAX || info.iform == XED_IFORM_MOV_MEMv_GPRv) &&
             info.base_reg == XED_REG_EBP) {
            for (xed_reg_enum_t live_reg : state.live_regs) {
               if (live_reg == info.reg0) {
                  /* frame store */
                  state.frame_index = info.memdisp;
                  if (verbose) {
                     fprintf(stderr, "[REBASIFY] 0x%zx frame store from %s to index %d\n",
                             inst->loc.vmaddr, xed_reg_enum_t2str(live_reg), *state.frame_index);
                  }
                  return 0;
               }
            }
         }

         /* look for frame load */
         if ((info.iform == XED_IFORM_MOV_GPRv_MEMv || info.iform == XED_IFORM_MOV_OrAX_MEMv) &&
             info.base_reg == XED_REG_EBP && info.memdisp == state.frame_index) {
            state.live_regs.insert(info.reg0);
            if (verbose) {
               fprintf(stderr, "[REBASIFY] 0x%zx frame load from index %d to %s\n",
                       inst->loc.vmaddr, *state.frame_index, xed_reg_enum_t2str(info.reg0));
            }
            return 0;
         }
         
         /* look for alias */
         if (info.iform == XED_IFORM_MOV_GPRv_GPRv_89 &&
             state.live_regs.find(info.reg1) != state.live_regs.end()) {
            state.live_regs.insert(info.reg0);
            if (verbose) {
               fprintf(stderr, "[REBASIFY] 0x%zx alias %s\n", inst->loc.vmaddr,
                       xed_reg_enum_t2str(info.reg0));
            }
            return 0;
         }
         
         /* otherwise handle inst thunk */
         handle_inst_thunk(inst, state, info);
      }
      return 0;

   default: abort();
   }
}

int Rebasify::handle_inst_thunk(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                                 const decode_info& info) const {
   /* custom rules with duplicate operands  */
   for (auto live_reg_it = state.live_regs.begin(); live_reg_it != state.live_regs.end();
        ++live_reg_it) {
      if (info.reg0 == *live_reg_it && info.reg1 == *live_reg_it) {
         switch (info.iclass) {
         case XED_ICLASS_XOR:
            state.live_regs.erase(live_reg_it);
            return 0;
            
         default:
            break;
         }
      }
   }
   
   /* check if reads from or writes to eax */
   switch (xed_decoded_inst_get_iclass(&inst->xedd)) {
   case XED_ICLASS_RET_NEAR:
   case XED_ICLASS_RET_FAR:
      state.reset();
      return 0;

   default:
      {
         std::size_t target = state.vmaddr;
         if (state.live_regs.find(info.base_reg) != state.live_regs.end()) {
            target += info.memdisp;
         }

         typename state_info::LiveRegs::iterator live_reg_it;
         if ((live_reg_it = state.live_regs.find(info.base_reg)) != state.live_regs.end() ||
             (live_reg_it = state.live_regs.find(info.reg1))     != state.live_regs.end()) {
            if (verbose) {
               fprintf(stderr, "[REBASIFY] 0x%zx ref dst=0x%zx\n", inst->loc.vmaddr,
                       target);
            }
            
            auto mov_inst = new MachO::Instruction<MachO::Bits::M32>
               (MachO::opcode::mov_r32_imm32(*live_reg_it));
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
            if (info.base_reg == *live_reg_it) {
               const unsigned dispbits =
                  xed_decoded_inst_get_memory_displacement_width_bits(&inst->xedd, 0);
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
   typename state_info::LiveRegs::iterator live_reg_it;
   if ((live_reg_it = state.live_regs.find(info.reg0)) != state.live_regs.end()) {
      switch (xed_decoded_inst_get_iclass(&inst->xedd)) {
      case XED_ICLASS_MOV:
      case XED_ICLASS_XOR:
      case XED_ICLASS_LEA:
         if (verbose) {
            fprintf(stderr, "[REBASIFY] 0x%zx register %s destroyed by instruction\n",
                    inst->loc.vmaddr, xed_reg_enum_t2str(*live_reg_it));
         }
         state.live_regs.erase(live_reg_it);
         return 0;

      default:
         break;
      }
   }

   return 0;
}

Rebasify::state_info::state_info(MachO::Archive<MachO::Bits::M32> *archive):
   archive(archive), segment(archive->segment(SEG_TEXT)), section(archive->section(SECT_TEXT)),
   text_it(section->content.begin())
{
   reset();
}

Rebasify::state_info::state_info(const state_info& other,
                                 const MachO::SectionBlob<MachO::Bits::M32> *target):
   state(other.state), vmaddr(other.vmaddr), live_regs(other.live_regs),
   frame_index(other.frame_index), archive(other.archive), segment(other.segment),
   section(other.section),
   text_it(std::find(section->content.begin(), section->content.end(), target))
{
#if 0
   // actually, it's ok if it's out of range
   if (text_it == section->content.end()) {
      throw std::invalid_argument("bad target blob");
   }
#endif
}
