#include <unordered_map>

#include "ast.hpp"
#include "util.hpp"

namespace zc {

   const char *UnaryExpr::kindstr() const {
      switch (kind_) {
      case Kind::UOP_ADDR: return "UOP_ADDR";
      case Kind::UOP_DEREFERENCE: return "UOP_DEREFERENCE";
      case Kind::UOP_POSITIVE:   return "UOP_POSITIVE";
      case Kind::UOP_NEGATIVE:   return "UOP_NEGATIVE";
      case Kind::UOP_BITWISE_NOT:          return "UOP_BITWISE_NOT";
      case Kind::UOP_LOGICAL_NOT: return "UOP_LOGICAL_NOT";
      case Kind::UOP_INC_PRE: return "UOP_INC_PRE";
      case Kind::UOP_INC_POST: return "UOP_INC_POST";
      case Kind::UOP_DEC_PRE: return "UOP_DEC_PRE";
      case Kind::UOP_DEC_POST: return "UOP_DEC_POST";
      }
   }
   
   void UnaryExpr::DumpNode(std::ostream& os) const {
      os << "UnaryExpr " << kindstr();
   }

   const char *BinaryExpr::kindstr() const {
      switch (kind_) {
      case Kind::BOP_LOGICAL_AND: return "BOP_LOGICAL_AND";
      case Kind::BOP_BITWISE_AND: return "BOP_BITWISE_AND";
      case Kind::BOP_LOGICAL_OR:  return "BOP_LOGICAL_OR";
      case Kind::BOP_BITWISE_OR:  return "BOP_BITWISE_OR";
      case Kind::BOP_BITWISE_XOR: return "BOP_BITWISE_XOR";
      case Kind::BOP_EQ:          return "BOP_EQ";
      case Kind::BOP_NEQ:         return "BOP_NEQ";
      case Kind::BOP_LT:          return "BOP_LT";
      case Kind::BOP_LEQ:         return "BOP_LEQ";
      case Kind::BOP_GT:          return "BOP_GT";
      case Kind::BOP_GEQ:         return "BOP_GEQ";
      case Kind::BOP_PLUS:        return "BOP_PLUS";
      case Kind::BOP_MINUS:       return "BOP_MINUS";
      case Kind::BOP_TIMES:       return "BOP_TIMES";
      case Kind::BOP_DIVIDE:      return "BOP_DIVIDE";
      case Kind::BOP_MOD:         return "BOP_MOD";
      default:              return "(invalid)";
      }
   }

   void BinaryExpr::DumpNode(std::ostream& os) const {
      os << "BinaryExpr " << kindstr();
   }

   void LiteralExpr::DumpNode(std::ostream& os) const {
      os << "LiteralExpr " << val_;
   }

   void StringExpr::DumpNode(std::ostream& os) const {
      os << "StringExpr " << *str_;
   }

   void IdentifierExpr::DumpChildren(std::ostream& os, int level, bool with_types) const {
      id_->Dump(os, level, with_types);
   }

   void CallExpr::DumpChildren(std::ostream& os, int level, bool with_types) const {
      fn()->Dump(os, level, with_types);
      for (auto param : *params()) { param->Dump(os, level, with_types); }
      // params()->Dump(os, level, with_types);
   }

   void CastExpr::DumpNode(std::ostream& os) const {
      os << "CastExpr ";
      type()->DumpNode(os);
   }
   void CastExpr::DumpChildren(std::ostream& os, int level, bool with_types) const {
      expr()->Dump(os, level, with_types);
   }

   void MembExpr::DumpNode(std::ostream& os) const {
      os << "MembExpr '" << *memb() << "' ";
      type()->DumpNode(os);
   }

   void MembExpr::DumpChildren(std::ostream& os, int level, bool with_types) const {
      expr()->Dump(os, level, with_types);
   }

   bool BinaryExpr::is_logical() const {
      switch (kind()) {
      case Kind::BOP_LOGICAL_AND:
      case Kind::BOP_LOGICAL_OR:
      case Kind::BOP_EQ:
      case Kind::BOP_NEQ:
      case Kind::BOP_LT:
      case Kind::BOP_LEQ:
      case Kind::BOP_GT:
      case Kind::BOP_GEQ:
         return true;
      default:
         return false;
      }
   }

   void SizeofExpr::DumpChildren(std::ostream& os, int level, bool with_types) const {
      std::visit(overloaded {
            [&](const ASTType *type) { type->Dump(os, level, with_types); },
            [&](const ASTExpr *expr) { expr->Dump(os, level, with_types); }
         }, variant_);
   }

   bool UnaryExpr::is_const() const {
      switch (kind()) {
      case Kind::UOP_POSITIVE:
      case Kind::UOP_NEGATIVE:
      case Kind::UOP_BITWISE_NOT:
      case Kind::UOP_LOGICAL_NOT:
         return true;

      case Kind::UOP_ADDR:
      case Kind::UOP_DEREFERENCE:
      case Kind::UOP_INC_PRE:
      case Kind::UOP_DEC_PRE:
      case Kind::UOP_INC_POST:
      case Kind::UOP_DEC_POST:
         return false;
      }
   }

   bool BinaryExpr::is_const() const {
      return lhs()->is_const() && rhs()->is_const();
   }
   
   void IndexExpr::DumpChildren(std::ostream& os, int level, bool with_types) const {
      base()->Dump(os, level, with_types);
      index()->Dump(os, level, with_types);      
   }

   LiteralExpr::LiteralExpr(intmax_t val, SourceLoc loc): ASTExpr(loc), val_(val) {
      type_ = IntegralType::Create(val, loc);
      // type_ = IntegralType::Create(IntegralType::IntKind::SPEC_LONG_LONG, loc);
   }

   /*** CAST ***/
   ASTExpr::Subexprs CallExpr::subexprs() {
      Subexprs subexprs;
      for (auto it = params()->begin(), end = params()->end(); it != end; ++it) {
         subexprs.push_back(&*it);
      }
      subexprs.push_back(&fn_);
      return subexprs;
   }

   bool ASTExpr::ExprEq(ASTExpr *other) {
      auto this_subexprs = subexprs(), other_subexprs = other->subexprs();
      if (this_subexprs.size() != other_subexprs.size()) { return false; }
      for (auto it = this_subexprs.begin(), end = this_subexprs.end(),
              other_it = other_subexprs.begin();
           it != end; ++it, ++other_it) {
         if (!(**it)->ExprEq(**other_it)) { return false; }
      }
      if (!type()->TypeEq(other->type())) { return false; }
      return true;
   }
   
}
