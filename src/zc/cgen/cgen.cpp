#include <numeric>
#include <string>
#include <unordered_set>
#include <ostream>

#include "ast.hpp"
#include "asm.hpp"
#include "optim.hpp"
#include "cgen.hpp"
#include "ralloc.hpp"
#include "emit.hpp"
#include "crt.hpp"

namespace zc {

   const Label crt_l_indcall("__indcall");
   const LabelValue crt_lv_indcall(&crt_l_indcall);   

   static Block dead_block;
   
   void Cgen(TranslationUnit *root, std::ostream& os, const char *filename) {
      
      CgenEnv env;
      root->CodeGen(env);

      // env.Serialize();
      env.Resolve();

      if (g_print.ralloc_info) {
         env.DumpAsm(std::cerr);
      }
      
      RegisterAllocator::Ralloc(env);
      
      /* resolve */
      env.Resolve();
      
      OptimizeIR(env);

      env.DumpAsm(os);
   }

   FunctionImpl::FunctionImpl(CgenEnv& env, Block *entry, Block *fin):
      entry_(entry), fin_(fin), stack_frame_(env.ext_env().frame()) {}
      
   BlockTransitions::BlockTransitions(const Transitions& vec): vec_(vec) {
      /* get mask of conditions */
      /* TODO: this doesn't work. */
   }

   const FrameValue *StackFrame::saved_fp() {
      return new FrameValue(sizes_, saved_fp_);
   }

   const FrameValue *StackFrame::saved_ra() {
      return new FrameValue(sizes_, saved_ra_);
   }
   
   void CgenExtEnv::Enter(Symbol *sym, const VarDeclarations *args) {
      sym_env_.Enter(sym);
      frame_ = StackFrame(args);
   }

   void CgenExtEnv::Exit() {
      sym_env_.Exit();
   }

   LabelValue *CgenExtEnv::LabelGen(const Symbol *id) const {
      std::string name = std::string("__CLABEL_") + *id;
      Label *label = new Label(*id);
      return new LabelValue(label);
   }

   VarSymInfo::VarSymInfo(const ExternalDecl *ext_decl): SymInfo() {
      decl_ = dynamic_cast<const VarDeclaration *>(ext_decl->decl());
      const ASTType *type = decl()->type();
      Label *label = new Label(std::string("_") + *ext_decl->sym());
      LabelValue *label_val = new LabelValue(label);

      if (type->kind() == ASTType::Kind::TYPE_FUNCTION ||
          type->kind() == ASTType::Kind::TYPE_ARRAY)
         {
         /* Functions and arrays _always_ behave as addresses */
            addr_ = val_ = label_val;
      } else {
         addr_ = label_val;
         val_ = new MemoryValue(label_val, type->bytes());
      }
   }

   VarSymInfo::VarSymInfo(const Value *addr, const VarDeclaration *decl): SymInfo(), addr_(addr) {
      if (decl->type()->kind() == ASTType::Kind::TYPE_FUNCTION ||
          decl->type()->kind() == ASTType::Kind::TYPE_ARRAY) {
         val_ = addr;
      } else {
         val_ = new MemoryValue(addr, decl->bytes());
      }
   }
   
   /*** STRING CONSTANTS ***/
   void StringConstants::Insert(const std::string& str) {
      if (strs_.find(str) == strs_.end()) {
         strs_[str] = new_label();
      }
   }

   LabelValue *StringConstants::Ref(const std::string& str) {
      Insert(str);

      return new LabelValue(strs_[str]);
   }

   Label *StringConstants::new_label() {
      return new Label(std::string("__") + std::string("strconst_") + std::to_string(counter_++));
   }

   /*** CODE GENERATION ***/

   void TranslationUnit::CodeGen(CgenEnv& env) {
      env.EnterScope();

      for (ExternalDecl *decl : *decls()) {
         decl->CodeGen(env);
      }
      
      env.ExitScope();
   }

   void ExternalDecl::CodeGen(CgenEnv& env) {
      /* enter global variable declaration into global scope */
      if (sym()) {
         SymInfo *info = new VarSymInfo(this);
         env.symtab().AddToScope(sym(), info);
      }
   }

   void FunctionDef::CodeGen(CgenEnv& env) {
      /* enter function into global scope */
      VarSymInfo *info = new VarSymInfo(this);
      const VarDeclarations *args = Type()->get_callable()->params();
      env.symtab().AddToScope(sym(), info);
      env.ext_env().Enter(sym(), args);

      /* enter parameters into subscope */
      env.EnterScope();      

      /* initialize stack frame */
      FrameGen(env.ext_env().frame());

      /* assign argument stack locations */
      for (const VarDeclaration *arg : *args) {
         VarSymInfo *info = env.ext_env().frame().next_arg(arg);
         env.symtab().AddToScope(arg->sym(), info);
      }
      
      Block *start_block = new Block(dynamic_cast<const LabelValue *>(info->addr())->label());
      Block *ret_block = new Block(start_block->label()->Prepend("__frameunset"));

      /* emit prologue and epilogue */
      emit_frameset(env, start_block);
      emit_frameunset(env, ret_block);
      
      Block *end_block = comp_stat()->CodeGen(env, start_block, false);

      /* TODO: check if unconditional return is already present before doing this? */      
      end_block->transitions().vec().push_back(new ReturnTransition(Cond::ANY)); 


      env.impls().impls().push_back(FunctionImpl(env, start_block, ret_block));

      env.ext_env().Exit();
      env.ExitScope();
   }
   
   Block *CompoundStat::CodeGen(CgenEnv& env, Block *block, bool new_scope) {
      if (new_scope) {
         env.EnterScope();
      }

      for (Declaration *decl : *decls()) {
         decl->Declare(env);
      }

      for (ASTStat *stat : *stats()) {
         block = stat->CodeGen(env, block);
      }
      
      if (new_scope) {
         env.ExitScope();
      }

      return block;
   }
   Block *ReturnStat::CodeGen(CgenEnv& env, Block *block) {
      /* generate returned expression */
      const Value *var;
      block = expr()->CodeGen(env, block, &var, ASTExpr::ExprKind::EXPR_RVALUE);
      block->instrs().push_back(new LoadInstruction(return_rv(expr()->type()->bytes()), var));
      
      /* append return transition */
      block->transitions().vec().push_back(new ReturnTransition(Cond::ANY));

      /* create new dummy block (this should be removed as dead code with optimization) */
      Block *dead_block = new Block(new_label());
      block->transitions().vec().push_back(new JumpTransition(dead_block, Cond::ANY));
      return dead_block;
   }

   Block *ExprStat::CodeGen(CgenEnv& env, Block *block) {
      /* pass out = nullptr, since result is thrown away */
      return expr()->CodeGen(env, block, nullptr, ASTExpr::ExprKind::EXPR_RVALUE);
   }

   Block *IfStat::CodeGen(CgenEnv& env, Block *block) {
      /* Create joining label and block. */
      Label *join_label = new_label("if_join");
      Block *join_block = new Block(join_label);

      /* Create if, else Blocks. */
      Label *if_label = new_label("if_block");
      Block *if_block = new Block(if_label);

      Label *else_label = new_label("else_block");
      Block *else_block = new Block(else_label);

      emit_predicate(env, block, cond(), if_block, else_block);
      
      /* Code generate if, else blocks */
      Block *if_end = if_body()->CodeGen(env, if_block);
      Block *else_end;
      if (else_body()) {
         else_end = else_body()->CodeGen(env, else_block);
      } else {
         else_end = else_block;
      }

      /* Transition if, else end blocks to join block */
      BlockTransition *join_transition = new JumpTransition(join_block, Cond::ANY);
      if_end->transitions().vec().push_back(join_transition);
      else_end->transitions().vec().push_back(join_transition);

      return join_block;
   }

   Block *LoopStat::CodeGen(CgenEnv& env, Block *block) {
      Label *body_label = new_label("loop_body");
      Block *body_block = new Block(body_label);
      BlockTransition *body_trans = new JumpTransition(body_block, Cond::ANY);
      
      Label *join_label = new_label("loop_join");
      Block *join_block = new Block(join_label);

      StatInfo *stat_info = new StatInfo(this, body_block, join_block);
      env.stat_stack().Push(stat_info);
      
      block->transitions().vec().push_back(body_trans);
      auto end_block = body()->CodeGen(env, body_block);
      end_block->transitions().vec().push_back(body_trans);
      
      env.stat_stack().Pop();
      
      return join_block;
   }

   Block *WhileStat::CodeGen(CgenEnv& env, Block *block) {
      Label *body_label = new_label("while_body");
      Block *body_block = new Block(body_label);
      
      Label *pred_label = new_label("while_pred");
      Block *pred_block = new Block(pred_label);
      BlockTransition *pred_trans = new JumpTransition(pred_block, Cond::ANY);

      Label *join_label = new_label("while_end");
      Block *join_block = new Block(join_label);

      /* push stat info onto statement stack */
      StatInfo *stat_info = new StatInfo(this, body_block, join_block);
      env.stat_stack().Push(stat_info);

      block->transitions().vec().push_back(pred_trans);
      emit_predicate(env, pred_block, pred(), body_block, join_block);

      body_block = body()->CodeGen(env, body_block);
      body_block->transitions().vec().push_back(pred_trans);

      block->transitions().vec().push_back(new JumpTransition(pred_block, Cond::ANY));

      env.stat_stack().Pop();

      return join_block;
   }

   Block *ForStat::CodeGen(CgenEnv& env, Block *block) {
      Label *cond_label = new_label("for_cond");
      Block *cond_block = new Block(cond_label);
      BlockTransition *cond_trans = new JumpTransition(cond_block, Cond::ANY);

      Label *after_label = new_label("for_after");
      Block *after_block = new Block(after_label);
      BlockTransition *after_trans = new JumpTransition(after_block, Cond::ANY);
      
      Label *body_label = new_label("for_body");
      Block *body_block = new Block(body_label);
      // BlockTransition *body_trans = new JumpTransition(body_block, Cond::NZ);

      Label *join_label = new_label("for_join");
      Block *join_block = new Block(join_label);
      // BlockTransition *join_trans = new JumpTransition(join_block, Cond::ANY);

      StatInfo *stat_info = new StatInfo(this, after_block, join_block);
      env.stat_stack().Push(stat_info);
      
      block = init()->CodeGen(env, block, nullptr, ASTExpr::ExprKind::EXPR_RVALUE);
      block->transitions().vec().push_back(cond_trans);

      emit_predicate(env, cond_block, pred(), body_block, join_block);
      
      block = body()->CodeGen(env, body_block);
      block->transitions().vec().push_back(after_trans);
      block = after()->CodeGen(env, after_block, nullptr, ASTExpr::ExprKind::EXPR_RVALUE);
      block->transitions().vec().push_back(cond_trans);
      
      env.stat_stack().Pop();
      
      return join_block;
   }

   Block *BreakStat::CodeGen(CgenEnv& env, Block *block) {
      StatInfo *stat_info = env.stat_stack().Find([](auto info){return info->stat()->can_break(); });
      block->transitions().vec().push_back(new JumpTransition(stat_info->exit(), Cond::ANY));
      return &dead_block;
   }

   Block *ContinueStat::CodeGen(CgenEnv& env, Block *block) {
      StatInfo *stat_info = env.stat_stack().Find([](auto info){return info->stat()->can_break(); });
      block->transitions().vec().push_back(new JumpTransition(stat_info->enter(), Cond::ANY));
      return &dead_block;
   }
   
   Block *GotoStat::CodeGen(CgenEnv& env, Block *block) {
      /* obtain label */
      const LabelValue *lv = env.ext_env().LabelGen(label_id()->id());
      block->instrs().push_back(new JumpInstruction(lv));
      return block;
   }

   Block *LabeledStat::CodeGen(CgenEnv& env, Block *block) {
      return stat()->CodeGen(env, block);
   }
   
   Block *LabelDefStat::CodeGen(CgenEnv& env, Block *block) {
      /* obtain label */
      const LabelValue *lv = env.ext_env().LabelGen(label_id()->id());
      
      /* create new block */
      Block *labeled_block = new Block(lv->label());

      /* link current block to labeled block */
      block->transitions().vec().push_back(new JumpTransition(labeled_block, Cond::ANY));
      block = labeled_block;
      return LabeledStat::CodeGen(env, block);
   }

   Block *AssignmentExpr::CodeGen(CgenEnv& env, Block *block, const Value **out,
                                  ExprKind mode) {
      assert(mode == ExprKind::EXPR_RVALUE);
      
      /* compute right-hand rvalue */
      const Value *lhs_lval, *rhs_rval;
      block = rhs()->CodeGen(env, block, &rhs_rval, ExprKind::EXPR_RVALUE);

      /* compute left-hand lvalue */
      block = lhs()->CodeGen(env, block, &lhs_lval, ExprKind::EXPR_LVALUE);
      
      /* assign */
      block->instrs().push_back(new LoadInstruction(&rv_hl, lhs_lval));
      const MemoryValue *memval = new MemoryValue(&rv_hl, type()->bytes());
      block->instrs().push_back(new LoadInstruction(memval, rhs_rval));

      /* propogate result */
      if (out) {
         *out = rhs_rval;
      }

      return block;
   }

   Block *UnaryExpr::CodeGen(CgenEnv& env, Block *block, const Value **out, ExprKind mode) {
      int bytes = expr()->type()->bytes();
      const Value *var;
      switch (kind()) {
      case Kind::UOP_ADDR:
         assert(mode == ExprKind::EXPR_RVALUE);
         /* get subexpression as lvalue */
         block = expr()->CodeGen(env, block, out, ExprKind::EXPR_LVALUE);
         break;
         
      case Kind::UOP_DEREFERENCE:
         switch (mode) {
         case ExprKind::EXPR_NONE: abort();
         case ExprKind::EXPR_LVALUE:
            block = expr()->CodeGen(env, block, out, ExprKind::EXPR_RVALUE);
            break;
         case ExprKind::EXPR_RVALUE:
            /* dereference address into _out_ */
            // var = new VariableValue(bytes);
            block = expr()->CodeGen(env, block, &var, ExprKind::EXPR_RVALUE);
            block->instrs().push_back(new LoadInstruction(&rv_hl, var));
            *out = new VariableValue(type()->bytes());
            block->instrs().push_back
               (new LoadInstruction(*out, new MemoryValue(&rv_hl, type()->bytes())));
            break;
         }
         break;
         
      case Kind::UOP_POSITIVE:
         return expr()->CodeGen(env, block, out, ExprKind::EXPR_RVALUE);         
         
      case Kind::UOP_NEGATIVE:
         {
            if (bytes == flag_size) {
               return expr()->CodeGen(env, block, out, ExprKind::EXPR_RVALUE);
            }
            
            /* select register to load into */
            const RegisterValue *reg;
            switch (bytes) {
            case byte_size: reg = &rv_a; break;
            case word_size: abort();
            case long_size: reg = crt_arg1(bytes); break;
            default:        abort();
            }

            block = expr()->CodeGen(env, block, &var, ExprKind::EXPR_RVALUE);
            block->instrs().push_back(new LoadInstruction(reg, var));

            /* negate result */
            switch (bytes) {
            case byte_size:
               /* neg */
               block->instrs().push_back(new NegInstruction());
               break;
            case word_size: abort();
            case long_size:
               /* call __ineg */
               emit_crt("__ineg", block);
               break;
            default: abort();
            }

            /* load result into destination */
            *out = new VariableValue(bytes);
            block->instrs().push_back(new LoadInstruction(*out, reg));
         }
         break;
         
      case Kind::UOP_BITWISE_NOT:
         {
            /* select register to load into */
            const RegisterValue *reg;
            switch (bytes) {
            case byte_size: reg = &rv_a; break;
            case word_size: abort();
            case long_size: reg = crt_arg1(bytes); break;
            default:        abort();
            }

            block = expr()->CodeGen(env, block, &var, ExprKind::EXPR_RVALUE);
            block->instrs().push_back(new LoadInstruction(reg, var));

            /* bitwise not */
            switch (bytes) {
            case byte_size:
               /* cpl */
               block->instrs().push_back(new CplInstruction());
               break;
            case word_size: abort();
            case long_size:
               /* call __inot */
               emit_crt("__inot", block);
               break;
            default: abort();
            }

            /* load result into destination */
            *out = new VariableValue(bytes);
            block->instrs().push_back(new LoadInstruction(*out, reg));
         }
         break;
         
      case Kind::UOP_LOGICAL_NOT:
         block = expr()->CodeGen(env, block, &var, ExprKind::EXPR_RVALUE);
         emit_logical_not(env, block, var, out);
         break;
         
      case Kind::UOP_INC_PRE:
      case Kind::UOP_DEC_PRE:
      case Kind::UOP_INC_POST:
      case Kind::UOP_DEC_POST:
         block = emit_incdec(env, block,
                             kind() == Kind::UOP_INC_PRE || kind() == Kind::UOP_INC_POST,
                             kind() == Kind::UOP_INC_PRE || kind() == Kind::UOP_DEC_PRE, expr(),
                             out);
         break;
      }

      return block;
   }

   Block *BinaryExpr::CodeGen(CgenEnv& env, Block *block, const Value **out,
                              ExprKind mode) {
      const Value *lhs_var, *rhs_var;
      
      switch (kind()) {
      case Kind::BOP_LOGICAL_AND:
      case Kind::BOP_LOGICAL_OR:
         {
            bool and_not_or = (kind() == Kind::BOP_LOGICAL_AND);
            const char *name = and_not_or ? "BOP_LOGICAL_AND" : "BOP_LOGICAL_OR";
            
            const Label *cont_label = new_label(name);
            Block *cont_block = new Block(cont_label);

            const Label *end_label = new_label(name);
            Block *end_block = new Block(end_label);

            /* evaluate lhs */
            block = lhs()->CodeGen(env, block, &lhs_var, ExprKind::EXPR_RVALUE);
            const FlagValue *flag1;
            emit_nonzero_test(env, block, lhs_var, &flag1);

            BlockTransition *cont_transition = new JumpTransition
               (cont_block, and_not_or ? flag1->cond_1() : flag1->cond_0());
            BlockTransition *end_transition =
               new JumpTransition(end_block, and_not_or ? flag1->cond_0() : flag1->cond_1());
            
            block->transitions().vec().push_back(cont_transition);
            block->transitions().vec().push_back(end_transition);

            /* Evaluate rhs */
            cont_block = rhs()->CodeGen(env, cont_block, &rhs_var, ExprKind::EXPR_RVALUE);
            const FlagValue *flag2;
            emit_nonzero_test(env, cont_block, rhs_var, &flag2);
            emit_convert_flag(cont_block, flag2, flag1);

            cont_block->transitions().vec().push_back(new JumpTransition(end_block, Cond::ANY));
            
            if (g_optim.bool_flag) {
               *out = flag1;
            } else {
               assert(type()->bytes() == byte_size);
               emit_booleanize_flag_byte(env, end_block, flag1, out);
            }
            
            return end_block;
         }

      case Kind::BOP_EQ:
      case Kind::BOP_NEQ:
         {
            bool eq_not_neq = (kind() == Kind::BOP_EQ);
            
            block = emit_binop(env, block, this, &lhs_var, &rhs_var);
            switch (lhs_var->size()) {
            case byte_size:
               /* ld a,<lhs>
                * cp a,<rhs>
                * _
                */
               block->instrs().push_back(new LoadInstruction(&rv_a, lhs_var));
               block->instrs().push_back(new CompInstruction(&rv_a, rhs_var));
               break;

            case word_size: abort();
            case long_size:
               /* ld hl,<lhs>
                * or a,a
                * sbc hl,<rhs>
                * add hl,<rhs>
                */
               block->instrs().push_back(new LoadInstruction(&rv_hl, lhs_var));
               block->instrs().push_back(new OrInstruction(&rv_a, &rv_a));
               block->instrs().push_back(new SbcInstruction(&rv_hl, rhs_var));
               block->instrs().push_back(new AddInstruction(&rv_hl, rhs_var));
               break;
               
            default: abort();
            }

            if (out == nullptr) {
               return block;
            }

            auto flag = new FlagValue(eq_not_neq ? Cond::NZ : Cond::Z,
                                 eq_not_neq ? Cond::Z : Cond::NZ);
            if (g_optim.bool_flag) {
               *out = flag; 
            } else {
               *out = new VariableValue(g_optim.bool_type()->bytes());
               block->instrs().push_back(new LoadInstruction(*out, &imm_b<0>));
               block->instrs().push_back(new JrInstruction(&imm_b<3>, flag->cond_0()));
               block->instrs().push_back(new IncInstruction(*out));
            }

            return block;
         }

      case Kind::BOP_LT:
      case Kind::BOP_GT:
      case Kind::BOP_LEQ:
      case Kind::BOP_GEQ:
         {
            bool lt_not_gt = (kind() == Kind::BOP_LT || kind() == Kind::BOP_LEQ);
            bool lg_not_eq = (kind() == Kind::BOP_LT || kind() == Kind::BOP_GT);
            bool rev_vars = (lt_not_gt == lg_not_eq);
            block = emit_binop(env, block, this, &lhs_var, &rhs_var);
            
            if (out == nullptr) { return block; }
            
            switch (lhs()->type()->bytes()) {
            case byte_size:
               /* ld a,<lhs>/<rhs>
                * cp a,<rhs>/<lhs>
                */
               block->instrs().push_back(new LoadInstruction(&rv_a, rev_vars ? lhs_var : rhs_var));
               block->instrs().push_back(new CompInstruction(&rv_a, rev_vars ? rhs_var : lhs_var));
               break;
                                  
            case word_size: abort();
            case long_size:
               /* ld hl,<lhs>
                * or a,a
                * sbc hl,<rhs>
                */
               block->instrs().push_back(new LoadInstruction(&rv_hl, rev_vars ? lhs_var : rhs_var));
               block->instrs().push_back(new OrInstruction(&rv_a, &rv_a));
               block->instrs().push_back(new SbcInstruction(&rv_hl, rev_vars ? rhs_var : lhs_var));
               break;
            
            default: abort();
            }

            const FlagValue *flag;
            if (lhs()->type()->int_type()->is_signed()) {
               flag = new FlagValue(Cond::P, Cond::M);
            } else {
               flag = new FlagValue(Cond::NC, Cond::C);
            }

            if (!lg_not_eq) {
               flag = flag->invert();
            }
            
            if (g_optim.bool_flag) {
               *out = flag;
            } else {
               emit_booleanize_flag(env, block, flag, out, type()->bytes());
            }
            
            return block;
         }
         break;

      case Kind::BOP_PLUS:
         block = emit_binop(env, block, this, &lhs_var, &rhs_var);
         block->instrs().push_back(new LoadInstruction(accumulator(lhs_var), lhs_var));
         block->instrs().push_back(new AddInstruction(accumulator(lhs_var), rhs_var));
         if (out) {
            *out = new VariableValue(lhs_var->size());
            block->instrs().push_back(new LoadInstruction(*out, accumulator(lhs_var)));
         }
         break;

      case Kind::BOP_MINUS:
         block = emit_binop(env, block, this, &lhs_var, &rhs_var);
         switch (lhs_var->size()) {
         case byte_size:
            /* ld a,<lhs>
             * sub a,<rhs>
             * ld <out>,a
             */
            block->instrs().push_back(new LoadInstruction(&rv_a, lhs_var));
            block->instrs().push_back(new SubInstruction(&rv_a, rhs_var));
            break;
            
         case word_size: abort();
         case long_size:
            /* or a,a
             * ld hl,<lhs>
             * sbc hl,<rhs>
             * ld <out>,hl
             */
            block->instrs().push_back(new OrInstruction(&rv_a, &rv_a));
            block->instrs().push_back(new LoadInstruction(&rv_hl, lhs_var));
            block->instrs().push_back(new SbcInstruction(&rv_hl, rhs_var));
            break;
         }

         if (out) {
            *out = new VariableValue(lhs_var->size());
            block->instrs().push_back(new LoadInstruction(*out, accumulator(lhs_var)));
         }
         
         break;

      case Kind::BOP_TIMES:
         block = emit_binop(env, block, this, &lhs_var, &rhs_var);
         switch (lhs_var->size()) {
         case byte_size:
            /* ld high(<vl>),<lhs>
             * ld low(<vl>),<rhs>
             * mlt <vl>
             * ld <out>,low(<vl>)
             */
            {
               auto vl = new VariableValue(long_size);
               block->instrs().push_back
                  (new LoadInstruction(new ByteValue(vl, ByteValue::Kind::BYTE_HIGH), lhs_var));
               block->instrs().push_back
                  (new LoadInstruction(new ByteValue(vl, ByteValue::Kind::BYTE_LOW), rhs_var));
               block->instrs().push_back(new MultInstruction(vl));
               if (out) {
                  *out = new VariableValue(byte_size);
                  block->instrs().push_back(new LoadInstruction
                                            (*out, new ByteValue(vl, ByteValue::Kind::BYTE_LOW)));
               }
            }
            break;
            
         case word_size: abort();
         case long_size:
            /* ld hl,<lhs>
             * ld bc,<rhs>
             * call __imulu
             * ld <out>,hl
             */
            {
               bool is_signed = lhs()->type()->int_type()->is_signed();
               block->instrs().push_back(new LoadInstruction(&rv_hl, lhs_var));
               block->instrs().push_back(new LoadInstruction(&rv_bc, rhs_var));
               emit_crt("__imul" + crt_suffix(is_signed), block);
               if (out) {
                  *out = new VariableValue(long_size);
                  block->instrs().push_back(new LoadInstruction(*out, &rv_hl));
               }
            }
            break;
         }
         break;

      case Kind::BOP_DIVIDE:
      case Kind::BOP_MOD:
         {
            bool div_not_mod = (kind() == Kind::BOP_DIVIDE);
            bool is_signed = lhs()->type()->int_type()->is_signed();
            block = emit_binop(env, block, this, &lhs_var, &rhs_var);
            block->instrs().push_back(new LoadInstruction(crt_arg1(lhs_var), lhs_var));
            block->instrs().push_back(new LoadInstruction(crt_arg2(rhs_var), rhs_var));
            emit_crt(crt_prefix(lhs_var) + "div" + crt_suffix(is_signed), block);
            if (out) {
               *out = new VariableValue(lhs_var->size());
               if (div_not_mod) {
                  block->instrs().push_back(new LoadInstruction(*out, crt_arg1(lhs_var)));
               } else {
                  block->instrs().push_back(new LoadInstruction(*out, crt_arg2(rhs_var)));
               }
            }
         }
         break;
         
      case Kind::BOP_BITWISE_AND:
      case Kind::BOP_BITWISE_OR:
      case Kind::BOP_BITWISE_XOR:
         block = emit_binop(env, block, this, &lhs_var, &rhs_var);
         switch (lhs_var->size()) {
         case byte_size:
            /* ld a,<lhs>
             * and/or/xor a,<rhs>
             * ld <out>,a
             */
            block->instrs().push_back(new LoadInstruction(&rv_a, lhs_var));
            switch (kind()) {
            case Kind::BOP_BITWISE_AND:
               block->instrs().push_back(new AndInstruction(&rv_a, rhs_var)); break;
            case Kind::BOP_BITWISE_OR:
               block->instrs().push_back(new OrInstruction(&rv_a, rhs_var));  break;
            case Kind::BOP_BITWISE_XOR:
               block->instrs().push_back(new XorInstruction(&rv_a, rhs_var)); break;
            default: abort();
            }
            if (out) {
               *out = new VariableValue(byte_size);
               block->instrs().push_back(new LoadInstruction(*out, &rv_a));
            }
            break;
            
         case word_size: abort();
         case long_size:
            /* ld hl,<lhs>
             * ld bc,<rhs>
             * call __iand/__ior/__ixor
             * ld <out>,hl
             */
            block->instrs().push_back(new LoadInstruction(&rv_hl, lhs_var));
            block->instrs().push_back(new LoadInstruction(&rv_bc, rhs_var));
            switch (kind()) {
            case Kind::BOP_BITWISE_AND: emit_crt("__iand", block); break;
            case Kind::BOP_BITWISE_OR:  emit_crt("__ior", block);  break;
            case Kind::BOP_BITWISE_XOR: emit_crt("__ixor", block); break;
            default: abort();
            }
            if (out) {
               *out = new VariableValue(long_size);
               block->instrs().push_back(new LoadInstruction(*out, &rv_hl));
            }
            break;
         }
         break;

      case Kind::BOP_COMMA:
         lhs()->CodeGen(env, block, nullptr, ExprKind::EXPR_RVALUE);
         rhs()->CodeGen(env, block, out, mode);
         break;
      }

      return block;
   }
   
   Block *LiteralExpr::CodeGen(CgenEnv& env, Block *block, const Value **out,
                               ExprKind mode) {
      const ImmediateValue *imm = new ImmediateValue(val(), type()->bytes());
      if (out) {
         *out = new VariableValue(imm->size());
         block->instrs().push_back(new LoadInstruction(*out, imm));
      }
      return block;
   }
   
   Block *StringExpr::CodeGen(CgenEnv& env, Block *block, const Value **out,
                              ExprKind mode) {
      assert(mode == ExprKind::EXPR_RVALUE); /* string constants aren't assignable */
      if (out) {
         *out = new VariableValue(long_size);
         block->instrs().push_back(new LoadInstruction(*out, env.strconsts().Ref(*str())));
      }
      return block;
   }
   
   Block *IdentifierExpr::CodeGen(CgenEnv& env, Block *block, const Value **out,
                                  ExprKind mode) {
      const SymInfo *id_info = env.symtab().Lookup(id()->id());

      if (out == nullptr) { return block; }
      
      if (mode == ExprKind::EXPR_LVALUE || type()->kind() == ASTType::Kind::TYPE_ARRAY) {
         /* obtain address of identifier */
         const Value *id_addr = dynamic_cast<const VarSymInfo *>(id_info)->addr();
         *out = new VariableValue(id_addr->size());
         block->instrs().push_back(new LeaInstruction(*out, id_addr));
      } else {
         const Value *id_rval = id_info->val();
         *out = new VariableValue(id_rval->size());         
         block->instrs().push_back(new LoadInstruction(*out, id_rval));
      }

      return block;
   }

   Block *CallExpr::CodeGen(CgenEnv& env, Block *block, const Value **out, ExprKind mode) {
      /* codegen params */
      for (auto it = params()->rbegin(), end = params()->rend(); it != end; ++it) {
         auto param = *it;
         int param_bytes = param->type()->Decay()->bytes();
         // const Value *param_var = new VariableValue(param_bytes);
         const Value *param_var;
         block = param->CodeGen(env, block, &param_var, ExprKind::EXPR_RVALUE);

         const Value *push_src;
         switch (param_bytes) {
         case byte_size:
            block->instrs().push_back(new LoadInstruction(&rv_a, param_var));
            push_src = &rv_af;
            break;
         case word_size: abort();
         case long_size:
            push_src = param_var;
            break;
         }
         
         block->instrs().push_back(new PushInstruction(push_src));
      }

      /* codegen callee */
      if (g_optim.direct_call && fn()->val_const(env)) {
         /* direct call */
         block->instrs().push_back(new CallInstruction(fn()->val_const(env)));
      } else {
         /* indirect call */
         const Value *fn_var;
         block = fn()->CodeGen(env, block, &fn_var, ExprKind::EXPR_RVALUE);
         block->instrs().push_back(new LoadInstruction(&rv_iy, fn_var));
         
         emit_crt("__indcall", block);
      }
      
      /* pop off args */
      for (ASTExpr *param : *params()) {
         /* TODO: this could be optimized. */
         block->instrs().push_back(new PopInstruction(&rv_de)); /* pop off into scrap register */
      }

      /* load result */
      if (out) {
         int ret_bytes = type()->bytes();
         *out = new VariableValue(ret_bytes);
         block->instrs().push_back(new LoadInstruction(*out, accumulator(ret_bytes)));
      }
      
      return block;
   }

   Block *CastExpr::CodeGen(CgenEnv& env, Block *block, const Value **out, ExprKind mode) {
      assert(mode == ExprKind::EXPR_RVALUE);

      int expr_bytes = expr()->type()->Decay()->bytes();
      int out_bytes = type()->Decay()->bytes();
      bool expr_signed = expr()->type()->Decay()->int_type()->is_signed();
      bool out_signed = type()->Decay()->int_type()->is_signed();

      assert(expr_bytes != word_size && out_bytes != word_size);

      if (expr_bytes == out_bytes) {
         /* NOTE: Can ignore sign due to how two's complement works. */
         return expr()->CodeGen(env, block, out, mode);
      }

      const Value *var;
      block = expr()->CodeGen(env, block, &var, mode);
      
      if (out == nullptr) { return block; }

      switch (out_bytes) {
      case flag_size:
         {
            emit_nonzero_test(env, block, var, (const FlagValue **) out);
         }
         break;
         
      case byte_size:
         {
            switch (expr_bytes) {
            case flag_size:
               emit_booleanize_flag_byte(env, block, dynamic_cast<const FlagValue *>(var), out);
               break;
               
            case word_size: abort();
            case long_size:
               {
                  *out = new VariableValue(out_bytes);            
                  auto low_byte = new ByteValue(var, ByteValue::Kind::BYTE_LOW);
                  block->instrs().push_back(new LoadInstruction(*out, low_byte));
               }
               break;
            }
         }
         break;

      case word_size: abort();
      case long_size:
         switch (expr_bytes) {
         case flag_size:
            emit_booleanize_flag_long(env, block, dynamic_cast<const FlagValue *>(var), out);
            break;
         case byte_size:
            {
               *out = new VariableValue(out_bytes);                         
               if (expr_signed) {
                  /* sign-extend
                   * rlc <var>
                   * sbc hl,hl
                   * rrc <var>
                   * ld l,<var>
                   */
                  block->instrs().push_back(new RlcInstruction(var));
                  block->instrs().push_back(new SbcInstruction(&rv_hl, &rv_hl));
                  block->instrs().push_back(new RrcInstruction(var));
               } else {
                  /* don't sign-extend 
                   * or a,a
                   * sbc hl,hl
                   * ld l,<var>
                   */
                  block->instrs().push_back(new OrInstruction(&rv_a, &rv_a));
                  block->instrs().push_back(new SbcInstruction(&rv_hl, &rv_hl));
               }
               block->instrs().push_back(new LoadInstruction(&rv_l, var));
               block->instrs().push_back(new LoadInstruction(*out, &rv_hl));
            }
            break;
         case word_size: abort();            
         default: abort();
         }
         break;
      }
      
      return block;
   }

   Block *MembExpr::CodeGen(CgenEnv& env, Block *block, const Value **out, ExprKind mode) {
      /* code generate struct as lvalue */
      const Value *struct_var;
      block = expr()->CodeGen(env, block, &struct_var, ExprKind::EXPR_LVALUE);

      if (out == nullptr) { return block; }
      *out = new VariableValue(type()->bytes());
      
      /* get internal offset */
      int offset = dynamic_cast<CompoundType *>(expr()->type())->offset(memb());
      ImmediateValue *imm = new ImmediateValue(offset, long_size);

      const Value *offset_var = new VariableValue(long_size);
      /* ld <off>,#off
       * ld hl,<struct_var>
       * add hl,<off>
       */
      block->instrs().push_back(new LoadInstruction(offset_var, imm));
      block->instrs().push_back(new LoadInstruction(&rv_hl, struct_var));
      block->instrs().push_back(new AddInstruction(&rv_hl, offset_var));
      
      switch (mode) {
      case ExprKind::EXPR_LVALUE:
         block->instrs().push_back(new LoadInstruction(*out, &rv_hl));
         break; /* already computed address */
         
      case ExprKind::EXPR_RVALUE:
         /* ld <out>,(hl) */
         block->instrs().push_back
            (new LoadInstruction(*out, new MemoryValue(&rv_hl, type()->bytes())));
         break;

      case ExprKind::EXPR_NONE: abort();
      }

      return block;
   }

   Block *SizeofExpr::CodeGen(CgenEnv& env, Block *block, const Value **out,
                              ExprKind mode) {
      if (out == nullptr) { return block; }
      *out = new VariableValue(type()->bytes());
      
      struct CodeGenVisitor {
         int operator()(const ASTType *type) { return type->bytes(); }
         int operator()(const ASTExpr *expr) { return expr->type()->bytes(); }
      };

      int imm = std::visit(CodeGenVisitor(), variant_);
      block->instrs().push_back(new LoadInstruction(*out, new ImmediateValue(imm, (*out)->size())));
      
      return block;
   }

   Block *IndexExpr::CodeGen(CgenEnv& env, Block *block, const Value **out, ExprKind mode) {
      /* generate index */
      const Value *index_var;
      block = index()->CodeGen(env, block, &index_var, ExprKind::EXPR_RVALUE);
      block->instrs().push_back(new LoadInstruction(&rv_hl, index_var));

      /* generate base */
      const Value *base_var;
      block = base()->CodeGen(env, block, &base_var, ExprKind::EXPR_LVALUE);

      if (out == nullptr) { return block; }

      block->instrs().push_back(new LoadInstruction(&rv_hl, index_var));
      block->instrs().push_back
         (new LoadInstruction(&rv_bc, new ImmediateValue(type()->bytes(), long_size)));
      emit_crt("__imuls", block);
      block->instrs().push_back(new AddInstruction(&rv_hl, base_var));

      switch (mode) {
      case ExprKind::EXPR_LVALUE:
         *out = new VariableValue(long_size);
         block->instrs().push_back(new LoadInstruction(*out, &rv_hl));
         break;
         
      case ExprKind::EXPR_RVALUE:
         *out = new VariableValue(type()->bytes());
         block->instrs().push_back
            (new LoadInstruction(*out, new MemoryValue(&rv_hl, type()->bytes())));
         break;
         
      case ExprKind::EXPR_NONE: abort();
      }
      return block;
   }


   
   /*** OTHER ***/

   static int label_counter = 0;

   const Register *return_register(const Value *val) { return return_register(val->size()); }
   const Register *return_register(int bytes) {
      switch (bytes) {
      case byte_size: return &r_a;
      case word_size: return &r_hl;
      case long_size: return &r_hl;
      default:        abort();
      }
   }

   const RegisterValue *return_rv(const Value *val) { return return_rv(val->size()); }
   const RegisterValue *return_rv(int bytes) {
      switch (bytes) {
      case byte_size: return &rv_a;
      case word_size: return &rv_hl;
      case long_size: return &rv_hl;
      default:        abort();
      }
   }


   const RegisterValue *accumulator(const Value *val) { return accumulator(val->size()); }
   const RegisterValue *accumulator(int bytes) {
      switch (bytes) {
      case byte_size: return &rv_a;
      case word_size: abort();
      case long_size: return &rv_hl;
      default:        abort();
      }
   }

   Label *new_label() { return new_label(std::string()); }
   Label *new_label(const std::string& prefix) {
      std::string s = std::string("__LABEL_") + prefix + "_" + std::to_string(label_counter++);
      return new Label(s);
   }



   /*** Other ***/
   void ByteRegister::Cast(Block *block, const Register *from) const {
      switch (from->kind()) {
      case Kind::REG_BYTE:
         break;
      case Kind::REG_MULTIBYTE:
         {
            const MultibyteRegister *from_mb = dynamic_cast<const MultibyteRegister *>(from);
            const ByteRegister *from_lsb = from_mb->regs().back();
            const RegisterValue *from_rv = new RegisterValue(from_lsb);
            block->instrs().push_back(new LoadInstruction(new RegisterValue(this), from_rv));
         }
         break;
      }
   }

   void MultibyteRegister::Cast(Block *block, const Register *from) const {
      switch (from->kind()) {
      case Kind::REG_BYTE:
         /* ensure long register being cast to does not contain given register. */
         assert(!contains(from));

         block->instrs().push_back(new LoadInstruction(new RegisterValue(this), &imm_l<0>));
         block->instrs().push_back(new LoadInstruction(new RegisterValue(regs()[1]),
                                                       new RegisterValue(from)));

         break;
         
      case Kind::REG_MULTIBYTE:
         break;
      }
   }
   
   /*** ASM DUMPS ***/
   void CgenEnv::DumpAsm(std::ostream& os) const {
      /* dump function implementations */
      impls().DumpAsm(os);

      /* dump string constants */
      strconsts().DumpAsm(os);
   }

   void FunctionImpls::DumpAsm(std::ostream& os) const {
      for (const FunctionImpl& impl : impls()) {
         impl.DumpAsm(os);
      }
   }

   void StringConstants::DumpAsm(std::ostream& os) const {
      for (auto it : strs_) {
         /* emit label */
         it.second->EmitDef(os);

         /* emit string */
         os << "\t.db\t\"" << it.first << "\",0" << std::endl;
      }
   }

   void CgenEnv::Serialize() {
      for (auto impl : impls().impls()) {
         impl.Serialize();
      }
   }

   void FunctionImpl::Serialize() {
      instrs_ = Instructions();
      Blocks visited;
      void (*fn)(Block *, Instructions&, Blocks& visited) = Block::Serialize;
      entry()->for_each_block(visited, fn, *instrs_, visited);
      fin()->for_each_block(visited, fn, *instrs_, visited);
   }

   void Block::Resolve(Block *block, const FunctionImpl *impl) {
      /* resolve instructions */
      Instructions resolved_instrs;
      for (auto instr : block->instrs()) {
         instr->Resolve(resolved_instrs);
      }
      block->instrs_ = resolved_instrs;

      /* resolve transitions */
      for (auto it = block->transitions().vec().begin(), end = block->transitions().vec().end();
           it != end; ++it) {
         *it = (*it)->Resolve(impl);
      }
      block->transitions().Prune();
   }

   void Block::Serialize(Block *block, Instructions& out, Blocks& visited) {
      /* label */
      out.push_back(new LabelInstruction(block->label()));

      /* instructions */
      std::copy(block->instrs().begin(), block->instrs().end(), std::inserter(out, out.end()));

      /* emit transitions */
      block->transitions().Serialize(out, visited);
   }

   void JumpTransition::Serialize(Instructions& out) {
      out.push_back(new JumpInstruction(new LabelValue(dst()->label()), cond()));
   }
   

   void Block::DumpAsm(Block *block, std::ostream& os, Blocks& visited) {
      /* emit label */
      block->label()->EmitDef(os);
      
      /* emit instructions */
      for (const Instruction *instr : block->instrs()) {
         instr->Emit(os);
      }

      block->transitions().DumpAsm(os, visited);
   }

   void FunctionImpl::Resolve() {
      Blocks visited;
      void (*fn)(Block *, const FunctionImpl *) = Block::Resolve;
      entry()->for_each_block(visited, fn, this);
      fin()->for_each_block(visited, fn, this);
   }
   
   void FunctionImpl::DumpAsm(std::ostream& os) const {
      Blocks visited;

      void (*fn)(Block *, std::ostream&, Blocks&) = Block::DumpAsm;
      
      entry()->for_each_block(visited, fn, os, visited);
      fin()->for_each_block(visited, fn, os, visited);      
   }

   std::ostream& operator<<(std::ostream& os, Cond cond) {
      switch (cond) {
      case Cond::Z: return os << "z";
      case Cond::NZ: return os << "nz";
      case Cond::C: return os << "c";
      case Cond::NC: return os << "nc";
      case Cond::P: return os << "p";
      case Cond::M: return os << "m";
      case Cond::ANY: return os;
      }
   }

   Cond invert(Cond cond) {
      switch (cond) {
      case Cond::Z: return Cond::NZ;
      case Cond::NZ: return Cond::Z;
      case Cond::C: return Cond::NC;
      case Cond::NC: return Cond::C;
      case Cond::P: return Cond::M;
      case Cond::M: return Cond::P;
      case Cond::ANY: return Cond::ANY;
      }
   }

   void BlockTransitions::Prune() {
      std::set<Cond> conds;
      auto it = vec().begin();
      auto end = vec().end();

      for (; it != end; ++it) {
         Cond cond = (*it)->cond();
         if (conds.find(cond) != conds.end()) {
            /* reduntant jump */
            it = vec_.erase(it);
         } else if (cond == Cond::ANY || conds.find(invert(cond)) != conds.end()) {
            ++it;
            break;
         } else {
            conds.insert(cond);
         }
      }

      /* erase unreachable transitions */
      vec_.erase(it, end);
   }

   void BlockTransitions::Serialize(Instructions& instrs, Blocks& visited) {
      /* NOTE: It has no transitions if it is __frameunset. */
      if (vec().empty()) {
         return;
      }

      auto it = vec().begin();
      auto last = --vec().end();
      for (; it != last; ++it) {
         (*it)->Serialize(instrs);
      }

      /* omit last transition but output block, if it hasn't been emitted already */
      auto dst = (*last)->dst();
      assert(dst);
      if (visited.find(dst) == visited.end()) {
         void (*fn)(Block *, Instructions& instrs, Blocks&) = Block::Serialize;
         dst->for_each_block(visited, fn, instrs, visited);
      } else {
         (*last)->Serialize(instrs);
      }
   }
   
   
   void BlockTransitions::DumpAsm(std::ostream& os, Blocks& visited)
      const {
      /* NOTE: It has no transitions if it is __frameunset. */
      if (vec().empty()) {
         return;
      }

      if (g_optim.minimize_transitions) {
         auto it = vec().begin();
         auto last = --vec().end();
         for (; it != last; ++it) {
            (*it)->DumpAsm(os);
         }
         
         /* omit last transition but output block, if it hasn't been emitted already */
         auto dst = (*last)->dst();
         assert(dst);
         if (visited.find(dst) == visited.end()) {
            void (*fn)(Block *, std::ostream& os, Blocks&) = Block::DumpAsm;      
            dst->for_each_block(visited, fn, os, visited);
         } else {
            (*last)->DumpAsm(os);
         }
      } else {
         for (auto trans : vec()) {
            trans->DumpAsm(os);
         }
      }
   }

   void JumpTransition::DumpAsm(std::ostream& os) const {
      os << "\tjp\t" << cond();
      if (cond() != Cond::ANY) { os << ","; }
      dst()->label()->EmitRef(os);
      os << std::endl;
   }

   BlockTransition *ReturnTransition::Resolve(const FunctionImpl *impl) {
      return new JumpTransition(impl->fin(), cond());
   }

   void CgenEnv::Resolve() {
      for (auto impl : impls().impls()) {
         impl.Resolve();
      }
   }

   /*** FRAME GEN & STACK FRAME ***/

   StackFrame::StackFrame(): sizes_(new FrameIndices()),
                             saved_fp_(sizes_->insert(sizes_->end(), long_size)),
                             saved_ra_(sizes_->insert(sizes_->end(), long_size)) {}

   
   StackFrame::StackFrame(const VarDeclarations *params):
      sizes_(new FrameIndices()),
      saved_fp_(sizes_->insert(sizes_->end(), long_size)),
      saved_ra_(sizes_->insert(sizes_->end(), long_size)) {
      for (const VarDeclaration *var_decl : *params) {
         /* TODO? */
      }
   }

   VarSymInfo *StackFrame::next_arg(const VarDeclaration *arg) {
      int bytes = arg->bytes();
      FrameIndices::iterator it;
      switch (bytes) {
      case byte_size:
         sizes_->insert(sizes_->end(), byte_size);
         it = sizes_->insert(sizes_->end(), byte_size);
         sizes_->insert(sizes_->end(), byte_size);         
         break;
      case word_size: abort();
      case long_size:
         it = sizes_->insert(sizes_->end(), long_size);
         break;
      default: abort();
      }

      auto offset = new FrameValue(sizes_, it);
      auto val = new IndexedRegisterValue(&rv_ix, IndexedRegisterValue::Index(offset));
      return new VarSymInfo(val, arg);
   }

   VarSymInfo *StackFrame::next_local(const VarDeclaration *decl) {
      auto it = sizes_->insert(saved_fp_, decl->bytes());
      auto offset = new FrameValue(sizes_, it);
      auto val = new IndexedRegisterValue(&rv_ix, IndexedRegisterValue::Index(offset));
      return new VarSymInfo(val, decl);
   }

   const Value *StackFrame::next_tmp(const Value *tmp) {
      auto it = sizes_->insert(sizes_->begin(), tmp->size());
      auto offset = new FrameValue(sizes_, it);
      return new IndexedRegisterValue(&rv_ix, IndexedRegisterValue::Index(offset));
   }

   const FrameValue *StackFrame::callee_bytes() { return new FrameValue(sizes_, saved_fp_); }
   const FrameValue *StackFrame::neg_callee_bytes() {
      return new FrameValue(sizes_, saved_fp_, true);
   }

    void FunctionDef::FrameGen(StackFrame& frame) const {
       comp_stat()->FrameGen(frame);
    }
 
   void CompoundStat::FrameGen(StackFrame& frame) const {
      for (const Declaration *decl : *decls()) {
         decl->FrameGen(frame);
         for (const ASTStat *stat : *stats()) {
            stat->FrameGen(frame);
         }
      }
   }

   void IfStat::FrameGen(StackFrame& frame) const {
      if_body()->FrameGen(frame);
      if (else_body()) {
         else_body()->FrameGen(frame);
      }
   }

   void WhileStat::FrameGen(StackFrame& frame) const {
      body()->FrameGen(frame);
   }

   void ForStat::FrameGen(StackFrame& frame) const {
      body()->FrameGen(frame);
   }

   void LoopStat::FrameGen(StackFrame& frame) const {
      body()->FrameGen(frame);
   }

   void VarDeclaration::FrameGen(StackFrame& frame) const {
      /* TODO: does this even need to happen anymore? */
      // frame.add_local(this);
   }

   void VarDeclaration::Declare(CgenEnv& env) {
      SymInfo *info = env.ext_env().frame().next_local(this);
      env.symtab().AddToScope(sym(), info);
   }

   void TypeDeclaration::Declare(CgenEnv& env) {
      dynamic_cast<DeclarableType *>(type())->Declare(env);
   }

   void Enumerator::Declare(CgenEnv& env) {
      const Value *val = new ImmediateValue(eval(), enum_type()->bytes());
      
      env.symtab().AddToScope
         (sym(),
          new ConstSymInfo(val, VarDeclaration::Create(sym(), true, enum_type(), loc())));
          
   }
   
   

   void EnumType::Declare(CgenEnv& env) {
      if (membs()) {
         for (auto memb : *membs()) {
            memb->Declare(env);
         }
      }
   }

#if 0
   int PointerType::bytes() const {
      return long_size;
   }

   int PointerType::bits() const {
      return long_size * 8;
   }

   int FunctionType::bytes() const {
      return long_size; /* actually treated as pointer */
   }

   int VoidType::bytes() const {
      return 0;
   }
#endif



   const Value *IdentifierExpr::val_const(const CgenEnv& env) const {
      const SymInfo *id_info = env.symtab().Lookup(id()->id());
      assert(id_info);
      const VarSymInfo *var_info = dynamic_cast<const VarSymInfo *>(id_info);
      if (var_info == nullptr) { return nullptr; }

      return dynamic_cast<const LabelValue *>(var_info->addr());
   }
}
