#pragma once

#include <string>
#include <sstream>
#include <unordered_set>

#include "loc.hh"
#include "typeinfo.hh"

using Symbols = std::unordered_set<std::string>;

#if 0
struct memloc {
   const char *basereg;
   int index;

   std::string memop() const {
      std::stringstream ss;
      ss << "[" << basereg << "+" << index << "]";
      return ss.str();
   }

   memloc operator+(int offset) const {
      return {basereg, index + offset};
   }

   memloc& operator+=(int offset) {
      index += offset;
      return *this;
   }

   memloc operator-(int offset) const {
      return {basereg, index - offset};
   }

   memloc& operator-=(int offset) {
      index -= offset;
      return *this;
   }

   void align(size_t a) {
      align_up<int>(index, a);
   }
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
   
   emit_arg_int(CXType param, const reg_group& reg): emit_arg(param), reg(reg) {}
   
   virtual const char *opcode() override;
   virtual std::string regstr() override { return reg.reg(param_width_64); }
};

struct emit_arg_real: emit_arg {
   unsigned xmm_idx;
   
   emit_arg_real(CXType param, unsigned xmm_idx): emit_arg(param), xmm_idx(xmm_idx) {}

   virtual const char *opcode() override;
   virtual std::string regstr() override { return std::string("xmm") + std::to_string(xmm_idx); }
};
#endif

template <typename Opcode>
std::ostream& emit_inst(std::ostream& os, Opcode&& opcode) {
   return os << "\t" << opcode << std::endl;
}

template <typename Opcode, typename OperandHead, typename... OperandTail>
std::ostream& emit_inst(std::ostream& os, Opcode&& opcode, OperandHead&& head,
                               OperandTail&&... tail) {
   return strjoin(os << "\t" << opcode << "\t", ",\t", head, tail...) << std::endl;
}
