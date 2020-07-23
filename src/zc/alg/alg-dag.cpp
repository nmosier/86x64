/* Transform expressions (and statements) into DAG. */

#include <algorithm>

#include "alg/alg-dag.hpp"
#include "ast.hpp"

namespace zc {

   void TranslationUnit::DAG() {
      for (auto decl : *decls()) {
         decl->DAG();
      }
   }

   void FunctionDef::DAG() {
      comp_stat()->DAG();
   }

   void ExprStat::DAG() { expr_ = expr()->DAG(); }
   void ReturnStat::DAG() { expr_ = expr()->DAG(); }
   void IfStat::DAG() { cond_ = cond()->DAG(); if_body()->DAG(); else_body()->DAG(); }
   void IterationStat::DAG() { pred_ = pred()->DAG(); body()->DAG(); }
   void ForStat::DAG() { IterationStat::DAG(); init_ = init()->DAG(); after_ = after()->DAG(); }

   ASTExpr *ASTExpr::DAG() {
      alg::DAGSet dagset;
      return transform(&ASTExpr::DAG_aux, dagset);
   }
   
   ASTExpr *ASTExpr::DAG_aux(alg::DAGSet& dagset) {
      auto it = std::find_if(dagset.begin(), dagset.end(),
                          [&](ASTExpr *dagexpr) {
                             return ExprEq(dagexpr);
                          });
      if (it != dagset.end()) {
         /* return expression in DAG set */
         return *it;
      } else {
         dagset.push_back(this);
         return this;
      }
   }
   
}
