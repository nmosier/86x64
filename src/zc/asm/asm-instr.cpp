#include <ostream>

#include "asm.hpp"
#include "cgen.hpp"
#include "alg.hpp"

namespace zc::z80 {
   
   using alg::ValueInserter;
    
    void Instruction::Emit(std::ostream& os) const {
       os << "\t" << name();
       if (cond_ || !operands_.empty()) {
          os << "\t";
       }
       if (cond_) {
          os << *cond_;
       }
       for (auto it = operands_.begin(), end = operands_.end(); it != end; ++it) {
          if (it != operands_.begin() || cond_) {
             os << ",";
          }
          (**it)->Emit(os);         
       }
       os << std::endl;
    }

    void JrInstruction::Emit(std::ostream& os) const {
       os << "\t" << name() << "\t";
       if (cond_) {
          os << *cond_ << ",";
       }
       os << "$+";
       dst()->Emit(os);
       os << std::endl;
    }

    bool Instruction::Eq(const Instruction *other) const {
       if (name() != other->name()) {
          return false;
       }

       auto it = operands_.begin(), other_it = other->operands_.begin(),
          end = operands_.end(), other_end = other->operands_.end();      
       for (; it != end && other_it != other_end; ++it, ++other_it) {
          if (!(**it)->Eq(**other_it)) {
             return false;
          }
       }

       return it == end && other_it == other_end;
    }

    bool Instruction::Match(const Instruction *other) const {
       if (name() != other->name()) {
          return false;
       }

       auto it = operands_.begin(), other_it = other->operands_.begin(),
          end = operands_.end(), other_end = other->operands_.end();      
       for (; it != end && other_it != other_end; ++it, ++other_it) {
          if (*it) {
             if (!(**it)->Match(**other_it)) {
                return false;
             }
          } else {
             it->send(**other_it);
          }
       }

       return it == end && other_it == other_end;
    }

    /*** GEN & USE ***/

    void BinaryInstruction::Kill(alg::ValueInserter vals) const { dst()->Kill(vals); }
    void BinaryInstruction::Gen(alg::ValueInserter vals) const {
       dst()->Gen(vals); 
      src()->Gen(vals);
   }

    void UnaryInstruction::Kill(ValueInserter vals) const { dst()->Kill(vals); }
    void UnaryInstruction::Gen(ValueInserter vals) const { dst()->Gen(vals); }

    void LoadInstruction::Kill(ValueInserter vals) const { dst()->Kill(vals); }
    void LoadInstruction::Gen(ValueInserter vals) const {
      src()->Gen(vals);
      auto mv = dynamic_cast<const MemoryValue *>(dst());
      if (mv) { mv->Gen(vals); } /* ugh, this is hideous */
   }

   void BitwiseInstruction::Kill(ValueInserter vals) const {
      if (!dst()->Eq(src())) { dst()->Kill(vals); }
   }
   void BitwiseInstruction::Gen(ValueInserter vals) const {
      if (!dst()->Eq(src())) { dst()->Gen(vals); src()->Gen(vals); }
   }

   void CallInstruction::Kill(ValueInserter vals) const {
      rv_hl.Kill(vals);
      rv_bc.Kill(vals);
      }
   void CallInstruction::Gen(ValueInserter vals) const {
      rv_hl.Gen(vals);
      rv_bc.Gen(vals);
   }
   
   void CplInstruction::Kill(ValueInserter vals) const { rv_a.Kill(vals); }
   void CplInstruction::Gen(ValueInserter vals) const { rv_a.Gen(vals); }

   void NegInstruction::Kill(ValueInserter vals) const { rv_a.Kill(vals); }
   void NegInstruction::Gen(ValueInserter vals) const { rv_a.Gen(vals); }

   void DjnzInstruction::Kill(ValueInserter vals) const { rv_b.Kill(vals); }
   void DjnzInstruction::Gen(ValueInserter vals) const { rv_b.Gen(vals); }

   void SbcInstruction::Kill(ValueInserter vals) const { dst()->Kill(vals); }
   void SbcInstruction::Gen(ValueInserter
                            vals) const {
      if (!dst()->Eq(src())) {
         dst()->Kill(vals);
         src()->Kill(vals);
      }
   }
   
   /*** COND KILL/GEN ***/
   void AddInstruction::Kill(alg::CondInserter conds) const {
      if (dst()->size() == byte_size) {
         std::copy(alg::all_conds.begin(), alg::all_conds.end(), conds);
      } else {
         *conds++ = Cond::C; *conds++ = Cond::NC;
      }
   }

   void IncDecInstruction::Kill(alg::CondInserter conds) const {
      if (dst()->size() == byte_size) {
         *conds++ = Cond::C, *conds++ = Cond::NC;
      }
   }
   
   /*** VARS ***/
   void Instruction::ReplaceVar(const VariableValue *var, const Value *with) {
      for (Values::iterator operand_it = operands_.begin(), operand_end = operands_.end();
           operand_it != operand_end;
           ++operand_it) {
         *operand_it = (**operand_it)->ReplaceVar(var, with);
      }
   }

   /*** RESOLUTION ***/

   void Instruction::Resolve(Instructions& out) {
      /* resolve all operands first */
      for (auto it = operands().begin(), end = operands().end(); it != end; ++it) {
         *it = (**it)->Resolve();
      }

      /* call auxiliary function */
      Resolve_(out);
   }

   
   void LoadInstruction::Resolve_(Instructions& out) {
      const Register *r1, *r2;
      int sz1, sz2;
      const RegisterValue rv1(&r1, &sz1);
      const RegisterValue rv2(&r2, &sz2);
      const LoadInstruction instr(&rv1, &rv2);
      if (instr.Match(this) && r1 != &r_sp) {
         if (sz1 == sz2 && (sz1 == word_size || sz1 == long_size)) {
            /* push rr1 \ pop rr2 */
            out.push_back(new PushInstruction(src()));
            out.push_back(new PopInstruction(dst()));
            return;
         }
      }

      out.push_back(this);
   }

   
}
