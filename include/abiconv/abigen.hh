#pragma once

#include <string>
#include <unordered_set>

#include "typeinfo.hh"

using Symbols = std::unordered_set<std::string>;

struct reg_group {
   const std::string reg_b;
   const std::string reg_w;
   const std::string reg_d;
   const std::string reg_q;
   
   const std::string& reg(reg_width width) const;
};

struct memloc {
   const char *basereg;
   int index;
};

struct emit_arg {
   CXType param;
   const reg_width param_width_32;
   reg_width param_width_64;
   bool sign;

   emit_arg(CXType param);
   
   virtual const char *opcode() = 0;
   virtual std::string regstr() = 0;
   std::string memop(const memloc& loc) const;

   void load(std::ostream& os, memloc& loc);
   void store(std::ostream& os, memloc& loc);
};

struct emit_arg_int: emit_arg {
   const reg_group& reg;
   
   emit_arg_int(CXType param, const reg_group& reg):
      emit_arg(param), reg(reg) {}
   
   virtual const char *opcode() override;
   virtual std::string regstr() override { return reg.reg(param_width_64); }
};

struct emit_arg_real: emit_arg {
   unsigned xmm_idx;
   
   emit_arg_real(CXType param, unsigned xmm_idx): emit_arg(param), xmm_idx(xmm_idx) {}

   virtual const char *opcode() override;
   virtual std::string regstr() override { return std::string("xmm") + std::to_string(xmm_idx); }
};
