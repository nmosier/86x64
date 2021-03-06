#pragma once

#include <unordered_set>
extern "C" {
#include <xed/xed-interface.h>
}
#include "command.hh"
#include "core/segment.hh"
#include "core/section.hh"

struct Rebasify: InOutCommand {
   struct state_info {
      /* 0 - init
       * 1 - seen 'call 0'
       * 2 - seen 'pop eax'
       * 3 - seen 'mov [ebp - idx], reg'
       */
      int state;
      std::size_t vmaddr;
      using LiveRegs = std::unordered_set<xed_reg_enum_t>;
      LiveRegs live_regs;
      std::optional<int> frame_index;

      MachO::Archive<MachO::Bits::M32> *archive = nullptr;
      MachO::Segment<MachO::Bits::M32> *segment = nullptr;
      MachO::Section<MachO::Bits::M32> *section = nullptr;
      typename MachO::Section<MachO::Bits::M32>::Content::iterator text_it;
      
      void reset();
      state_info(MachO::Archive<MachO::Bits::M32> *archive);
      state_info(const state_info& other, const MachO::SectionBlob<MachO::Bits::M32> *target);

      bool operator==(const state_info& other) const;
   };

   struct decode_info {
      typename MachO::Section<MachO::Bits::M32>::Content::iterator text_it;
      xed_reg_enum_t reg0, reg1;
      xed_iform_enum_t iform;
      xed_iclass_enum_t iclass;
      unsigned nmemops;
      xed_reg_enum_t base_reg;
      int64_t memdisp;

      decode_info(const xed_decoded_inst_t& xedd,
                  typename MachO::Section<MachO::Bits::M32>::Content::iterator text_it);
   };

   bool verbose = false;

   virtual const char *optstring() const override { return "hv"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"verbose", no_argument, nullptr, 'v'},
              {0}};
   }
   virtual int opthandler(int optchar) override;
   virtual std::string optusage() const override { return "[-hv]"; }
   virtual int work() override;
   Rebasify(): InOutCommand("rebasify") {}

   void handle_insts(MachO::Archive<MachO::Bits::M32> *archive) const;
   int handle_inst(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                   const decode_info& info) const;
   int handle_inst_thunk(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                         const decode_info& info) const;
};
