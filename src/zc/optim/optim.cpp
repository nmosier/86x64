#include <cstring>

#include "optim.hpp"
#include "ast.hpp"
#include "cgen.hpp"
#include "peephole.hpp"
#include "ralloc.hpp"
#include "config.hpp"

namespace zc {

   void OptimizeAST(TranslationUnit *root) {
      if (g_optim.reduce_const) {
         root->ReduceConst();
      }
   }

   void OptimizeIR(CgenEnv& env) {
      /* pass 0: register allocation */
      // RegisterAllocator::Ralloc(env);

      /* pass 1: peephole optimization */
      if (g_optim.peephole) {
         for (FunctionImpl& impl : env.impls().impls()) {
            for (PeepholeOptimization& optim : peephole_optims) {
               optim.Pass(&impl);
            }
         }

         if (g_print.peephole_stats) {
            std::cerr << "peephole-stats:" << std::endl << "NAME\t\tHITS\tTOTAL\tSAVED" << std::endl;
            for (PeepholeOptimization& optim : peephole_optims) {
               optim.Dump(std::cerr);
               std::cerr << std::endl;
            }
         }
      }
   }

   /*** OPTIMIZATION SETTINGS ***/

   CgenOptimInfo g_optim ({
      {"join-vars", &CgenOptimInfo::join_vars},
      {"reduce-const", &CgenOptimInfo::reduce_const},
      {"peephole", &CgenOptimInfo::peephole},
      {"bool-flag", &CgenOptimInfo::bool_flag},
      {"minimize-transitions", &CgenOptimInfo::minimize_transitions},
      {"direct-call", &CgenOptimInfo::direct_call},
      {"DAG", &CgenOptimInfo::DAG},
   });

   
   /*** CONSTANTS ***/

   void TranslationUnit::ReduceConst() { for (auto decl : *decls()) { decl->ReduceConst(); } }

   ASTExpr *ASTExpr::ReduceConst() {
      if (is_const()) {
         auto expr = LiteralExpr::Create(int_const(), type(), loc());
         return expr;
      } else {
         ReduceConst_rec();
         return this;
      }
   }

   void ASTUnaryExpr::ReduceConst_rec() {
      expr_ = expr()->ReduceConst();
   }

   void ASTBinaryExpr::ReduceConst_rec() {
      lhs_ = lhs()->ReduceConst();
      rhs_ = rhs()->ReduceConst();
   }

   void FunctionDef::ReduceConst() {
      comp_stat()->ReduceConst();
   }

   ASTStat *CompoundStat::ReduceConst() {
      for (auto it = stats()->begin(), end = stats()->end(); it != end; ++it) {
         *it = (*it)->ReduceConst();
      }
      return this;
   }

   ASTStat *ExprStat::ReduceConst() {
      if (expr()->is_const()) {
         return NoStat::Create(loc());
      } else {
         expr_ = expr()->ReduceConst();
         return this;
      }
   }

   ASTStat *ReturnStat::ReduceConst() {
      expr_ = expr()->ReduceConst();
      return this;
   }

   ASTStat *IfStat::ReduceConst() {
      if_body_ = if_body()->ReduceConst();
      if (else_body_) {
         else_body_ = else_body()->ReduceConst();
      }
      
      if (cond()->is_const()) {
         intmax_t val = cond()->int_const();
         return val ? if_body() : else_body();
      } else {
         cond_ = cond()->ReduceConst();
         return this;
      }
   }

   ASTStat *IterationStat::ReduceConst() {
      pred_ = pred()->ReduceConst();
      body_ = body()->ReduceConst();

      return ReduceConst_aux();
   }

   ASTStat *WhileStat::ReduceConst_aux() {
      if (pred()->is_const()) {
         if (pred()->int_const()) {
            return LoopStat::Create(body(), loc());
         } else {
            return NoStat::Create(loc());
         }
      } else {
         return this;
      }
   }

   ASTStat *ForStat::ReduceConst_aux() {
      init_ = init()->ReduceConst();
      after_ = after()->ReduceConst();

      if (pred()->is_const()) {
         if (pred()->int_const()) {
            /* convert to simpler while loop */
            auto body_stats = new ASTStats();
            body_stats->push_back(body());
            body_stats->push_back(ExprStat::Create(after(), loc()));
            auto body = CompoundStat::Create(new Declarations(loc()), body_stats, loc());
            auto loop = LoopStat::Create(body, loc());
            auto pre = ExprStat::Create(init(), loc());
            auto stats = new ASTStats();
            return CompoundStat::Create(new Declarations(), stats, loc());
         } else {
            return ExprStat::Create(init(), loc());
         }
      } else {
         return this;
      }
   }

   ASTStat *LabeledStat::ReduceConst() {
      stat_ = stat()->ReduceConst();
      return this;
   }

   void CallExpr::ReduceConst_rec() {
      fn_ = fn()->ReduceConst();

      for (auto it = params()->begin(), end = params()->end(); it != end; ++it) {
         *it = (*it)->ReduceConst();
      }
   }
   
}
