#ifndef __AST_HPP
#error "include ast.h, not ast/ast-stat.h directly"
#endif

#ifndef __AST_STAT_HPP
#define __AST_STAT_HPP

#include "asm-fwd.hpp"

namespace zc {

   class ASTStat: public ASTNode {
   public:
      virtual bool can_break() const { return false; }
      virtual bool can_continue() const { return false; }

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) = 0;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) = 0;
      virtual void FrameGen(StackFrame& frame) const = 0;

      /* AST Transformations */
      virtual ASTStat *ReduceConst() = 0;
      virtual void DAG() = 0;
      
   protected:
      ASTStat(const SourceLoc& loc): ASTNode(loc) {}
   };
   //   template <> const char *ASTStats::name() const;   

   class CompoundStat: public ASTStat {
   public:
      Declarations *decls() const { return decls_; }
      ASTStats *stats() const { return stats_; }

      template <typename... Args>
      static CompoundStat *Create(Args... args) {
         return new CompoundStat(args...);
      }

      virtual void DumpNode(std::ostream& os) const override { os << "CompoundStat"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override { return TypeCheck(env, true); }
      void TypeCheck(SemantEnv& env, bool scoped);

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override {
         return CodeGen(env, block, false);
      }
      Block *CodeGen(CgenEnv& env, Block *block, bool new_scope);
      virtual void FrameGen(StackFrame& env) const override;

      /* AST Transformation */
      virtual ASTStat *ReduceConst() override;
      virtual void DAG() override { for (auto stat : *stats()) { stat->DAG(); } }
      
   protected:
      Declarations *decls_;
      ASTStats *stats_;
      
      CompoundStat(Declarations *decls, ASTStats *stats, const SourceLoc& loc):
         ASTStat(loc), decls_(decls), stats_(stats) {}
   };
   
   class ExprStat: public ASTStat {
   public:
      ASTExpr *expr() const { return expr_; }

      template <typename... Args>
      static ExprStat *Create(Args... args) {
         return new ExprStat(args...);
      }

      virtual void DumpNode(std::ostream& os) const override { os << "ExprStat"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {
         expr_->Dump(os, level, with_types);
      }

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      virtual void FrameGen(StackFrame& env) const override {}

      /* AST Transformation */
      virtual ASTStat *ReduceConst() override;
      virtual void DAG() override;
      
   protected:
      ASTExpr *expr_;

      template <typename... Args>
      ExprStat(ASTExpr *expr, Args... args): ASTStat(args...), expr_(expr) {}
   };

   /* NOTE: abstract */
   class JumpStat: public ASTStat {
   public:
   protected:
      template <typename... Args> JumpStat(Args... args): ASTStat(args...) {}
   };

   class ReturnStat: public JumpStat {
   public:
      ASTExpr *expr() const { return expr_; }

      template <typename... Args> static ReturnStat *Create(Args... args) {
         return new ReturnStat(args...);
      }
      
      virtual void DumpNode(std::ostream& os) const override { os << "ReturnStat"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {
         expr_->Dump(os, level, with_types);
      }

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      virtual void FrameGen(StackFrame& env) const override {}    

      /* AST Transformation */
      virtual ASTStat *ReduceConst() override;
      virtual void DAG() override;
      
   protected:
      ASTExpr *expr_;

      template <typename... Args>
      ReturnStat(ASTExpr *expr, Args... args): JumpStat(args...), expr_(expr) {}
   };

   class BreakStat: public JumpStat {
   public:
      virtual void DumpNode(std::ostream& os) const override { os << "BreakStat"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {}

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *) override;
      virtual void FrameGen(StackFrame& frame) const override {}

      /* AST Transformation */
      virtual ASTStat *ReduceConst() override { return this; }
      virtual void DAG() override {}
      
      template <typename... Args>
      static BreakStat *Create(Args... args) { return new BreakStat(args...); }
      
   private:
      template <typename... Args>
      BreakStat(Args... args): JumpStat(args...) {}
   };

   class ContinueStat: public JumpStat {
   public:
      virtual void DumpNode(std::ostream& os) const override { os << "BreakStat"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {}

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *) override;
      virtual void FrameGen(StackFrame& frame) const override {}

      /* AST Transformation */
      virtual ASTStat *ReduceConst() override { return this; }
      virtual void DAG() override {}
      
      template <typename... Args>
      static ContinueStat *Create(Args... args) { return new ContinueStat(args...); }
      
   private:
      template <typename... Args>
      ContinueStat(Args... args): JumpStat(args...) {}
   };

   /* NOTE: Abstract */
   class SelectionStat: public ASTStat {
   public:
   protected:
      template <typename... Args> SelectionStat(Args... args): ASTStat(args...) {}
   };

   class IfStat: public SelectionStat {
   public:
      ASTExpr *cond() const { return cond_; }
      ASTStat *if_body() const { return if_body_; }
      ASTStat *else_body() const { return else_body_; }

      template <typename... Args> static IfStat *Create(Args... args) {
         return new IfStat(args...);
      }

      virtual void DumpNode(std::ostream& os) const override { os << "IfStat"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_type) const override {
         cond()->Dump(os, level, with_type);
         if_body()->Dump(os, level, with_type);
         if (else_body() != nullptr) {
            else_body()->Dump(os, level, with_type);
         }
      }

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      virtual void FrameGen(StackFrame& env) const override;          
      
      /* AST Transformation */
      virtual ASTStat *ReduceConst() override;
      virtual void DAG() override;

   protected:
      ASTExpr *cond_;
      ASTStat *if_body_;
      ASTStat *else_body_;

      template <typename... Args>
      IfStat(ASTExpr *cond, ASTStat *if_body, ASTStat *else_body, Args... args):
         SelectionStat(args...), cond_(cond), if_body_(if_body), else_body_(else_body) {}
   };

   /* NOTE: Abstract */
   class IterationStat: public ASTStat {
   public:
      ASTExpr *pred() const { return pred_; }
      ASTStat *body() const { return body_; }

      virtual bool can_break() const override { return true; }
      virtual bool can_continue() const override { return true; }
      
      /* AST Optimization */
      virtual ASTStat *ReduceConst() override;
      virtual ASTStat *ReduceConst_aux() = 0;
      virtual void DAG() override;

   protected:
      ASTExpr *pred_;
      ASTStat *body_;
      
      template <typename... Args>
      IterationStat(ASTExpr *pred, ASTStat *body, Args... args):
         ASTStat(args...), pred_(pred), body_(body) {}
   };

   /**
    * Infinite loop. NOTE: No corresponding construct in C; used for optimizations.
    */
   class LoopStat: public ASTStat {
   public:
      ASTStat *body() const { return body_; }

      virtual bool can_break() const override { return true; }
      virtual bool can_continue() const override { return true; }

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override {}
      
      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      virtual void FrameGen(StackFrame& frame) const override;

      /* AST Transformation */
      virtual ASTStat *ReduceConst() override { return this; }
      virtual void DAG() override { body()->DAG(); }

      template <typename... Args>
      static LoopStat *Create(Args... args) { return new LoopStat(args...); }
      
   private:
      ASTStat *body_;

      template <typename... Args>
      LoopStat(ASTStat *body, Args... args): ASTStat(args...), body_(body) {}
   };

   class WhileStat: public IterationStat {
   public:
      template <typename... Args>
      static WhileStat *Create(Args... args) {
         return new WhileStat(args...);
      }

      virtual void DumpNode(std::ostream& os) const override { os << "WhileStat"; }
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      virtual void FrameGen(StackFrame& env) const override;

      /* AST Transformation */
      virtual ASTStat *ReduceConst_aux() override;
      
   protected:
      template <typename... Args>
      WhileStat(Args... args): IterationStat(args...) {}
   };

   class ForStat: public IterationStat {
   public:
      ASTExpr *init() const { return init_; }
      ASTExpr *after() const { return after_; }

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      virtual void FrameGen(StackFrame& env) const override;
      
      /* AST Transformation */
      virtual ASTStat *ReduceConst_aux() override;
      virtual void DAG() override;

      template <typename... Args>
      static ForStat *Create(Args... args) { return new ForStat(args...); }
      
   private:
      ASTExpr *init_;
      ASTExpr *after_;
      
      template <typename...  Args>
      ForStat(ASTExpr *init, ASTExpr *cond, ASTExpr *after, Args... args):
         IterationStat(cond, args...), init_(init), after_(after) {}
   };

   class GotoStat: public ASTStat {
   public:
      Identifier *label_id() const { return label_id_; }
      
      template <typename... Args>
      static GotoStat *Create(Args... args) {
         return new GotoStat(args...);
      }

      virtual void DumpNode(std::ostream& os) const override;
      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override {}

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      virtual void FrameGen(StackFrame& env) const override {}

      /* AST Transformation */
      virtual ASTStat *ReduceConst() override { return this; }
      virtual void DAG() override {}
      
   private:
      Identifier *label_id_;
      
      template <typename... Args>
      GotoStat(Identifier *label_id, Args... args): ASTStat(args...), label_id_(label_id) {}
   };

   /**
    * Abstract class representing a labeled statement.
    */
   class LabeledStat: public ASTStat {
   public:
      ASTStat *stat() const { return stat_; }

      virtual void DumpChildren(std::ostream& os, int level, bool with_types) const override;

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      
      /* AST Transformation */
      virtual ASTStat *ReduceConst() override;
      virtual void DAG() override { stat()->DAG(); }

   protected:
      ASTStat *stat_;

      template <typename... Args>
      LabeledStat(ASTStat *stat, Args... args): ASTStat(args...), stat_(stat) {}
   };

   class LabelDefStat: public LabeledStat {
   public:
      Identifier *label_id() const { return label_id_; }
      
      template <typename... Args>
      static LabelDefStat *Create(Args... args) {
         return new LabelDefStat(args...);
      }

      virtual void DumpNode(std::ostream& os) const override;

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override;

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override;
      virtual void FrameGen(StackFrame& env) const override {}
      
   private:
      Identifier *label_id_;

      template <typename... Args>
      LabelDefStat(Identifier *label_id, Args... args): LabeledStat(args...), label_id_(label_id) {}
   };

   class NoStat: public ASTStat {
   public:
      template <typename... Args>
      static NoStat *Create(Args... args) {
         return new NoStat(args...);
      }

      /* Semantic Analysis */
      virtual void TypeCheck(SemantEnv& env) override {}

      /* Code Generation */
      virtual Block *CodeGen(CgenEnv& env, Block *block) override { return block; }
      virtual void FrameGen(StackFrame& env) const override {}

      /* AST Transformation */
      virtual ASTStat *ReduceConst() override { return this; }
      virtual void DAG() override {}

   private:
      template <typename... Args>
      NoStat(Args... args): ASTStat(args...) {}
   };
   


}

#endif
