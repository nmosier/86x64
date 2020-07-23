#include <algorithm>

#include "asm.hpp"

namespace zc::z80 {
   
   const ByteRegister r_a("a"), r_b("b"), r_c("c"), r_d("d"), r_e("e"), r_f("f"), r_h("h"),
      r_l("l"), r_ixh("ixh"), r_ixl("ixl"), r_iyh("iyh"), r_iyl("iyl");

   const MultibyteRegister r_af(MultibyteRegister::ByteRegs{&r_a, &r_f}, "af"),
      r_bc(MultibyteRegister::ByteRegs{&r_b, &r_c}, "bc"),
      r_de(MultibyteRegister::ByteRegs{&r_d, &r_e}, "de"),
      r_hl(MultibyteRegister::ByteRegs{&r_h, &r_l}, "hl"),
      r_ix(MultibyteRegister::ByteRegs{&r_ixh, &r_ixl}, "ix"),
      r_iy(MultibyteRegister::ByteRegs{&r_iyh, &r_iyl}, "iy"),
      r_sp(MultibyteRegister::ByteRegs{nullptr, nullptr}, "sp");

   const RegisterValue rv_a(&r_a, byte_size),
      rv_b(&r_b),
      rv_c(&r_c),
      rv_d(&r_d),
      rv_e(&r_e),
      rv_f(&r_f),
      rv_h(&r_h),
      rv_l(&r_l),
      rv_ixh(&r_ixh),
      rv_ixl(&r_ixl),
      rv_iyh(&r_iyh),
      rv_iyl(&r_iyl);
   const RegisterValue rv_af(&r_af),
      rv_bc(&r_bc),
      rv_de(&r_de),
      rv_hl(&r_hl),
      rv_ix(&r_ix),
      rv_iy(&r_iy),
      rv_sp(&r_sp);

   bool MultibyteRegister::contains(const Register *reg) const {
      if (reg->kind() != Kind::REG_BYTE) {
         return false;
      }

      const ByteRegister *byte_reg = dynamic_cast<const ByteRegister *>(reg);
      return std::find(regs().begin(), regs().end(), byte_reg) != regs().end();
   }

   bool MultibyteRegister::Eq_aux(const MultibyteRegister *other) const {
      for (auto it = regs().begin(), other_it = other->regs().begin(), end = regs().end();
           it != end; ++it, ++other_it) {
         if (*it == *other_it) { continue; }
         if (*it == nullptr || *other_it == nullptr) { return false; }
         if (!(*it)->Eq(*other_it)) {
            return false;
         }
      }
      return true;
   }

   bool Register::Eq(const Register *other) const {
      return strcmp(name(), other->name()) == 0 && size() == other->size() && Eq_(other);
   }
   
}
