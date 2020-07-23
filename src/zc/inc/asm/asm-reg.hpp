#ifndef __ASM_HPP
#error "include \"asm.hpp\""
#endif

#ifndef __ASM_REG_HPP
#define __ASM_REG_HPP

#include <array>
#include <ostream>

#include "asm-fwd.hpp"
#include "asm/asm-mode.hpp"
#include "cgen-fwd.hpp"

namespace zc::z80 {

   extern const ByteRegister r_a, r_b, r_c, r_d, r_e, r_f, r_h, r_l, r_ixh,
      r_ixl, r_iyh, r_iyl;
   extern const MultibyteRegister r_af, r_bc, r_de, r_hl, r_ix, r_iy, r_sp;

   extern const RegisterValue rv_a, rv_b, rv_c, rv_d, rv_e, rv_f, rv_h, rv_l, rv_ixh,
      rv_ixl, rv_iyh, rv_iyl;
   extern const RegisterValue rv_af, rv_bc, rv_de, rv_hl, rv_ix, rv_iy, rv_sp;
   
   /*************
    * REGISTERS *
    *************/

   /**
    * Base class representing a register (single- or multi-byte registers).
    */
   class Register {
   public:
      enum class Kind {REG_BYTE, REG_MULTIBYTE};
      virtual Kind kind() const = 0;
      const char *name() const { return name_; }
      virtual int size() const = 0;
      virtual const Register *high() const = 0;
      virtual const Register *low() const = 0;
      
      void Emit(std::ostream& os) const { os << name(); }
      virtual void Cast(Block *block, const Register *from) const = 0;

      bool Eq(const Register *other) const;

      void Dump(std::ostream& os) const { os << name(); }

   protected:
      const char *name_;

      virtual bool Eq_(const Register *other) const = 0;

      constexpr Register(const char *name): name_(name) {}
   };

   template <class Derived>
   class Register_: public Register {
   public:
   protected:
      virtual bool Eq_(const Register *other) const override {
         auto derived = dynamic_cast<const Derived *>(other);
         return derived ? Eq_aux(derived) : false;
      }

      virtual bool Eq_aux(const Derived *other) const = 0;
      
      template <typename... Args>
      Register_(Args... args): Register(args...) {}
   };

   class ByteRegister: public Register_<ByteRegister> {
   public:
      virtual Kind kind() const override { return Kind::REG_BYTE; }
      virtual int size() const override { return byte_size; }
      virtual const Register *high() const override {
         throw std::logic_error("attempted to take high byte of single-byte register");
      }
      virtual const Register *low() const override { return this; }

      virtual void Cast(Block *block, const Register *from) const override;
      
      template <typename... Args>
      constexpr ByteRegister(Args... args): Register_(args...) {}
      
   protected:
      virtual bool Eq_aux(const ByteRegister *other) const override { return true; }
   };

   class MultibyteRegister: public Register_<MultibyteRegister> {
   public:
      virtual int size() const override { return long_size; }
      typedef std::vector<const ByteRegister *> ByteRegs;
      // typedef std::array<const ByteRegister *, word_size> ByteRegs;
      virtual Kind kind() const override { return Kind::REG_MULTIBYTE; }
      const ByteRegs& regs() const { return regs_; }
      bool contains(const Register *reg) const;
      virtual const Register *high() const override { return regs()[0]; }
      virtual const Register *low() const override { return regs()[1]; }
      
      virtual void Cast(Block *block, const Register *from) const override;


      template <typename... Args>
      constexpr MultibyteRegister(Args... args): Register_(args...) {}
      template <typename... Args>
      constexpr MultibyteRegister(const ByteRegs& regs, Args... args):
         Register_(args...), regs_(regs) {}
      
   protected:
      const ByteRegs regs_;

      virtual bool Eq_aux(const MultibyteRegister *other) const override;
   };

}

#endif
