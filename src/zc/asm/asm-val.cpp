#include <stdexcept>

#include "asm.hpp"
#include "cgen.hpp"

namespace zc::z80 {

   int VariableValue::id_counter_ = 0;

   /*** EMIT ***/
   
   void ImmediateValue::Emit(std::ostream& os) const {
      os << *imm_;
   }

   void LabelValue::Emit(std::ostream& os) const {
      label()->EmitRef(os);
   }

   void RegisterValue::Emit(std::ostream& os) const {
      reg()->Emit(os);
   }

   void VariableValue::Emit(std::ostream& os) const {
      os << (force_reg() ? 'r' : 'v');
      switch (size()) {
      case byte_size: os << 'b'; break;
      case word_size: os << 'w'; break;
      case long_size: os << 'l'; break;
      default: abort();
      }
      os << id();
   }

   void IndexedRegisterValue::Emit(std::ostream& os) const {
      val()->Emit(os);
      os << "+";
      os << (int) index(); 
   }

   void FrameValue::Emit(std::ostream& os) const { os << (int) index(); }

   void OffsetValue::Emit(std::ostream& os) const {
      base()->Emit(os);
      os << "+";
      os << offset();
   }

   void MemoryValue::Emit(std::ostream& os) const {
      os << "(";
      addr()->Emit(os);
      os << ")";
   }

   /*** ADD ***/
   
   Value *ImmediateValue::Add(const intmax_t& offset) const {
      return new ImmediateValue(imm() + offset, size());
   }

   Value *LabelValue::Add(const intmax_t& offset) const {
      return new OffsetValue(this, offset, size());
   }

   Value *RegisterValue::Add(const intmax_t& offset) const {
      return new IndexedRegisterValue(this, offset);
   }

   Value *IndexedRegisterValue::Add(const intmax_t& offset) const {
      return new IndexedRegisterValue(val(), index() + offset);
   }

   Value *FrameValue::Add(const intmax_t& offset) const {
      if (offset > std::numeric_limits<int8_t>::max() ||
          offset < std::numeric_limits<int8_t>::min()) {
         throw std::logic_error("offset too large");
      }
      return new FrameValue(indices_, indices_->insert(pos_, offset));
   }

   Value *OffsetValue::Add(const intmax_t& new_offset) const {
      return new OffsetValue(base(), offset() + new_offset, size());
   }

   Value *MemoryValue::Add(const intmax_t& offset) const {
      throw std::logic_error("attempted to offset a memory value");
   }

   MemoryValue *MemoryValue::Next(int next_size) const {
      return new MemoryValue(addr()->Add(size()), next_size);
   }

   MemoryValue *MemoryValue::Prev(int next_size) const {
      return new MemoryValue(addr()->Add(-next_size), next_size);
   }


   ImmediateValue::ImmediateValue(const intmax_t& imm): Value_(long_size), imm_(imm) {
      if (imm <= byte_max) {
         size_ = byte_size;
      } else if (imm <= word_max) {
         size_ = word_size;
      } else if (imm <= long_max) {
         size_ = long_size;
      }
   }

   /*** MATCHING ***/
   bool Value::Match(const Value *to) const {
      if (size_) {
         if (size() != to->size()) {
            return false;
         }
      } else {
         size_.send(to->size());
      }
      return Match_(to);
   }
   
   bool ImmediateValue::Match_aux(const ImmediateValue *to) const {
      if (imm_) {
         return imm() == to->imm();
      } else {
         imm_.send(to->size());
      return true;
      }
   }
   
   bool RegisterValue::Match_aux(const RegisterValue *to) const {
      if (reg_) {
         return reg()->Eq(to->reg());
      } else {
         reg_.send(to->reg());
         return true;
      }
   }

   bool IndexedRegisterValue::Match_aux(const IndexedRegisterValue *to) const {
      if (val_) {
         if (!val()->Match(to->val())) {
            return false;
         }
      } else {
         val_.send(to->val());
      }

      if (index_) {
         if (index() != to->index()) {
            return false;
         }
      } else {
         index_.send(to->index());
      }

      return true;
   }

   bool MemoryValue::Match_aux(const MemoryValue *to) const {
      if (addr_) {
         return addr()->Match(to->addr());
      } else {
         addr_.send(to->addr());
         return true;
      }
   }

   bool FlagValue::Match_aux(const FlagValue *to) const {
      if (cond_0_) {
         if (cond_0() != to->cond_0()) {
            return false;
         }
      } else {
         cond_0_.send(to->cond_0());
      }
      if (cond_1_) {
         if (cond_1() != to->cond_1()) {
            return false;
         }
      } else {
         cond_1_.send(to->cond_1());
      }
      return true;
   }
   
   bool VariableValue::Match_aux(const VariableValue *to) const {
      if (id_) {
         if (id() != to->id()) {
            return false;
         }
      } else {
         id_.send(to->id());
      }
      
      if (force_reg_) {
         if (force_reg() != to->force_reg()) {
            return false;
         }
      } else {
         force_reg_.send(to->force_reg());
      }
      return true;
   }
   
   /*** OTHER ***/
   int8_t FrameValue::index() const {
      int8_t i = std::reduce(indices_->begin(), pos_, 0);
      return negate_ ? -i : i;
   }

   int8_t IndexedRegisterValue::index() const {
      struct IndexVisitor {
         int8_t operator()(int8_t i) const { return i; }
         int8_t operator()(const FrameValue *fv) const { return fv->index(); }
      };
      return std::visit(IndexVisitor(), *index_);
   }

   const Value *ByteValue::Resolve() const {
      if (dynamic_cast<const VariableValue *>(all())) {
         return this;
      } else {
         switch (kind()) {
         case Kind::BYTE_HIGH: return all()->high();
         case Kind::BYTE_LOW:  return all()->low();
         }
      }
   }

   std::ostream& operator<<(std::ostream& os, ByteValue::Kind kind) {
      using Kind = ByteValue::Kind;
      switch (kind) {
      case Kind::BYTE_HIGH: return os << "high";
      case Kind::BYTE_LOW: return os << "low"; 
      }
   }

   void ByteValue::Emit(std::ostream& os) const {
      os << kind() << "("; all()->Emit(os); os << ")";
   }
   
   void FlagValue::Emit(std::ostream& os) const {
      os << "[" << cond_0() << "|" << cond_1() << "]";
   }

}
