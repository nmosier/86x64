
#include "ast.hpp"
#include "util.hpp"

namespace zc {

const int indent_step = 2;

   void ASTNode::Dump(std::ostream& os, int level, bool with_types) const {
      indent(os, level);
      os << "#" << loc_ << " ";
      DumpNode(os);
      if (with_types) {
         os << " ";
         DumpType(os);
      }
      os << std::endl;
      DumpChildren(os, level + indent_step, with_types);
   }

   void ASTBinaryExpr::DumpChildren(std::ostream& os, int level, bool with_types) const {
      lhs_->Dump(os, level, with_types);
      rhs_->Dump(os, level, with_types);
   }

   void ASTUnaryExpr::DumpChildren(std::ostream& os, int level, bool with_types) const {
      expr_->Dump(os, level, with_types);
   }


   void ASTExpr::DumpType(std::ostream& os) const {
      type_->DumpNode(os);
   }

   //template <> const char *ASTStats::name() const { return "ASTStats"; }
   
}
