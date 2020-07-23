#ifndef __AST_HPP
#error "include ast.h, not ast-base.h directly"
#endif

#ifndef __AST_BASE_HPP
#define __AST_BASE_HPP

#include <set>

#include "semant.hpp"

namespace zc {

   std::ostream& indent(std::ostream& os, int level);

   class ASTNode {
   public:
      const SourceLoc& loc() const { return loc_; }
      virtual void Dump(std::ostream& os, int level, bool with_types) const;
      virtual void DumpNode(std::ostream& os) const {}
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const {}
      virtual void DumpType(std::ostream& os) const {}

   protected:
      SourceLoc loc_;
      ASTNode(const SourceLoc& loc): loc_(loc) {}
   };

   template <class Node>
   class ASTNodeVec: public ASTNode {
   public:
      typedef std::vector<Node *> Vec;
      Vec& vec() { return vec_; }
      const Vec& vec() const { return vec_; }

      template <typename... Args> static ASTNodeVec<Node> *Create(Args... args) {
         return new ASTNodeVec<Node>(args...);
      }

      virtual void DumpNode(std::ostream& os) const override { os << name(); }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {
         for (const Node *node : vec_) {
            node->Dump(os, level, with_types);
         }
      }

      template <typename... Args>
      void TypeCheck(SemantEnv& env, Args... args) {
         for (Node *node : vec_) {
            node->TypeCheck(env, args...);
         }
      }

   protected:
      Vec vec_;
      const char *name() const { return "ASTNodeVec"; }
      
      template <typename... Args> ASTNodeVec(Vec vec, Args... args): ASTNode(args...), vec_(vec) {}
      template <typename... Args> ASTNodeVec(Node *node, Args... args):
         ASTNode(args...), vec_(1, node) {}
      template <class OtherNode, typename Func, typename... Args>
      ASTNodeVec(ASTNodeVec<OtherNode> *other, Func func, Args... args): ASTNode(args...) {
         vec_.resize(other->vec().size());
         std::transform(other->vec().begin(), other->vec().end(), vec_.begin(), func);
      }
      template <class InputIt, typename Func, typename... Args>
      ASTNodeVec(InputIt begin, InputIt end, Func func, Args... args): ASTNode(args...) {
         std::transform(begin, end, std::back_inserter(vec_), func);
      }
      template <typename... Args> ASTNodeVec(Args... args): ASTNode(args...) {}
      
   };

   
}

#endif
