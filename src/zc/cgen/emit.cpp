#include "cgen.hpp"
#include "emit.hpp"
#include "asm.hpp"

namespace zc {

   void emit_nonzero_test(CgenEnv& env, Block *block, const Value *in, const FlagValue **out) {
      int bytes = in->size();
      switch (bytes) {
      case flag_size:
         *out = dynamic_cast<const FlagValue *>(in);
         return;
         
      case byte_size:
         /* ld a,<out>
          * or a,a
          */
         block->instrs().push_back(new LoadInstruction(new RegisterValue(&r_a), in));
         block->instrs().push_back
            (new OrInstruction(new RegisterValue(&r_a), new RegisterValue(&r_a)));
         *out = new FlagValue(Cond::Z, Cond::NZ);
         return;
         
      case word_size: abort();
      case long_size:
         /* ld hl,<in> 
          * call _icmpzero
          */
         block->instrs().push_back(new LoadInstruction(&rv_hl, in));
         emit_crt("__icmpzero", block);
         *out = new FlagValue(Cond::Z, Cond::NZ);         
         return;
      default: abort();
      }
   }

   /* NOTE: Doesn't return block because it's sealed by transitions.
    */
   void emit_predicate(CgenEnv& env, Block *block, ASTExpr *expr, Block *take, Block *leave) {
      // const Value *var = new VariableValue(expr->type()->bytes());
      const Value *var;
      block = expr->CodeGen(env, block, &var, ASTExpr::ExprKind::EXPR_RVALUE);
      const FlagValue *flag;
      emit_nonzero_test(env, block, var, &flag);
      
      /* transitions */
      block->transitions().vec().push_back(new JumpTransition(take, flag->cond_1()));
      block->transitions().vec().push_back(new JumpTransition(leave, flag->cond_0()));
   }

   void emit_logical_not(CgenEnv& env, Block *block, const Value *in, const Value **out) {
      Instructions& is = block->instrs();
      
      *out = new VariableValue(byte_size);
      
      int bytes = in->size();
      switch (bytes) {
      case flag_size:
         {
            const FlagValue *in_flag = dynamic_cast<const FlagValue *>(in);
            assert(in_flag);
            *out = in_flag->invert();
            return;            
         }
         
      case byte_size:
         /* ld a,<in>
          * cp a,1
          * ld a,0
          * adc a,a
          * ld <out>,a
          */
         is.push_back(new LoadInstruction(&rv_a, in));
         is.push_back(new CompInstruction(&rv_a, &imm_b<0>));
         is.push_back(new LoadInstruction(&rv_a, &imm_b<0>));
         is.push_back(new AdcInstruction(&rv_a, &rv_a));
         is.push_back(new LoadInstruction(*out, &rv_a));
         return;

      case word_size: abort();
         
      case long_size:
         /* or a,a
          * sbc hl,hl  ; ld hl,0
          * sbc hl,<in> ; CF <- <in> is nonzero
          * sbc hl,hl ; ld hl,-1 (c), ld hl,0 (nc)
          * inc hl
          * ld <out>,hl
          */
         is.push_back(new OrInstruction(&rv_a, &rv_a));
         is.push_back(new SbcInstruction(&rv_hl, &rv_hl));
         is.push_back(new SbcInstruction(&rv_hl, in));
         is.push_back(new SbcInstruction(&rv_hl, &rv_hl));
         is.push_back(new IncInstruction(&rv_hl));
         is.push_back(new LoadInstruction(*out, &rv_hl));
         break;
      }
   }

    void emit_booleanize_flag(CgenEnv& env, Block *block, const FlagValue *in, const Value **out,
                              int bytes) {
        switch (bytes) {
        case flag_size: abort();
        case byte_size:
            emit_booleanize_flag_byte(env, block, in, out);
            break;
        case word_size: abort();
        case long_size:
            emit_booleanize_flag_long(env, block, in, out);
            break;
        default: abort();
        }
    }

   void emit_booleanize_flag_byte(CgenEnv& env, Block *block, const FlagValue *in,
                                  const Value **out) {
      *out = &rv_a;
      switch (in->cond_0()) {
      case Cond::ANY: abort();
      case Cond::Z:
      case Cond::NZ:
         /* ld a,0
          * jr z/nz,_
          * inc a
          * _
          */
         block->instrs().push_back(new LoadInstruction(&rv_a, &imm_b<0>));
         block->instrs().push_back(new JrInstruction(new ImmediateValue(3), in->cond_0()));
         block->instrs().push_back(new IncInstruction(&rv_a));
         break;
         
      case Cond::NC:
         /* sbc a,a
          * neg
          */
         block->instrs().push_back(new SbcInstruction(&rv_a, &rv_a));
         block->instrs().push_back(new NegInstruction());
         break;
         
      case Cond::C:
         /* sbc a,a
          * inc a
          */
         block->instrs().push_back(new SbcInstruction(&rv_a, &rv_a));
         block->instrs().push_back(new IncInstruction(&rv_a));
         break;

      case Cond::P:
         emit_crt("___sftob", block);
         break;
         
      case Cond::M:
         emit_crt("___sftobn", block);
         break;
         
      }
   }

   void emit_booleanize_flag_long(CgenEnv& env, Block *block, const FlagValue *in,
                                  const Value **out) {
      switch (in->cond_0()) {
      case Cond::ANY: abort();
      case Cond::Z:
      case Cond::NZ:
         /* ld <out>,0 
          * jr z/nz,_ 
          * inc <out>
          * _
          */
         *out = new VariableValue(long_size);         
         block->instrs().push_back(new LoadInstruction(*out, &imm_l<0>));
         block->instrs().push_back(new JrInstruction(new ImmediateValue(3), in->cond_0()));
         block->instrs().push_back(new IncInstruction(*out));
         break;
         
      case Cond::NC:
         /* sbc hl,hl
          * call __ineg
          */
         *out = &rv_hl;
         block->instrs().push_back(new SbcInstruction(&rv_hl, &rv_hl));
         emit_crt("__ineg", block);
         break;
         
      case Cond::C:
         /* sbc hl,hl
          * inc hl
          */
         *out = &rv_hl;
         block->instrs().push_back(new SbcInstruction(&rv_hl, &rv_hl));
         block->instrs().push_back(new IncInstruction(&rv_hl));
         break;

      case Cond::P:
         emit_crt("___sftol", block);
         break;

      case Cond::M:
         emit_crt("___sftoln", block);
         break;
      }
      
   }

   Block *emit_incdec(CgenEnv& env, Block *block, bool inc_not_dec, bool pre_not_post,
                      ASTExpr *subexpr, const Value **out) {
      const Value *lval = new VariableValue(long_size);
      block = subexpr->CodeGen(env, block, &lval, ASTExpr::ExprKind::EXPR_LVALUE);
      Instructions& is = block->instrs();
      int bytes = subexpr->type()->bytes();

      if (out) {
         *out = new VariableValue(bytes);
      }

      /* preincrement if necessary */
      if (pre_not_post) {
         switch (bytes) {
         case byte_size:
            /* ld a,(<lval>)
             * inc/dec a
             * ld (<lval>),a
             * ld <out>,a
             * NOTE: Target for peephole optimization.
             */
            {
               const MemoryValue *memval = new MemoryValue(lval, subexpr->type()->bytes());
               is.push_back(new LoadInstruction(&rv_a, memval));
               if (inc_not_dec) { is.push_back(new IncInstruction(&rv_a)); }
               else { is.push_back(new DecInstruction(&rv_a)); }
               is.push_back(new LoadInstruction(memval, &rv_a));
               if (out) {
                  is.push_back(new LoadInstruction(*out, &rv_a));
               }
            }
            break;
         case word_size: abort();
         case long_size:
            /* ld hl,<lval>
             * ld <rval>,(hl)
             * inc/dec <rval>
             * ld (hl),<rval>
             * ld <out>,<rval>
             */
            {
               const Value *rval = new VariableValue(long_size);
               const MemoryValue *memval = new MemoryValue(&rv_hl, long_size);
               is.push_back(new LoadInstruction(&rv_hl, lval));
               is.push_back(new LoadInstruction(rval, memval));
               if (inc_not_dec) { is.push_back(new IncInstruction(rval)); }
               else { is.push_back(new DecInstruction(rval)); }
               is.push_back(new LoadInstruction(memval, rval));
               if (out) { is.push_back(new LoadInstruction(*out, rval)); }
            }
            break;
         }
      } else {
         /* post inc/dec */
         switch (bytes) {
         case byte_size:
            /* ld a,(<lval>)
             * ld <out>,a
             * inc/dec a
             * ld (<lval>),a
             */
            {
               const MemoryValue *memval = new MemoryValue(lval, byte_size);
               is.push_back(new LoadInstruction(&rv_a, memval));
               is.push_back(new LoadInstruction(*out, &rv_a));
               if (inc_not_dec) { is.push_back(new IncInstruction(&rv_a)); }
               else { is.push_back(new DecInstruction(&rv_a)); }
               is.push_back(new LoadInstruction(memval, &rv_a));
            }
            break;
         case word_size: abort();
         case long_size:
            /* ld hl,<lval>
             * ld <rval>,(hl)
             * ld <out>,<rval>
             * inc/dec <rval>
             * ld (hl),<rval>
             */
            {
               const Value *rval = new VariableValue(long_size);
               const MemoryValue *memval = new MemoryValue(&rv_hl, long_size);
               is.push_back(new LoadInstruction(&rv_hl, lval));
               is.push_back(new LoadInstruction(rval, memval));
               is.push_back(new LoadInstruction(*out, rval));
               if (inc_not_dec) { is.push_back(new IncInstruction(rval)); }
               else { is.push_back(new DecInstruction(rval)); }
               is.push_back(new LoadInstruction(memval, rval));
            }
            break;
         }
      }

      return block;
   }

   
   
   Block *emit_binop(CgenEnv& env, Block *block, ASTBinaryExpr *expr, const Value **out_lhs,
                     const Value **out_rhs) {
      /* evaluate lhs */
      block = expr->lhs()->CodeGen(env, block, out_lhs, ASTBinaryExpr::ExprKind::EXPR_RVALUE);
      
      /* evaluate rhs */
      block = expr->rhs()->CodeGen(env, block, out_rhs, ASTBinaryExpr::ExprKind::EXPR_RVALUE);

      return block;
   }

   void emit_crt(const std::string& name, Block *block) {
      const LabelValue *lv = g_crt.val(name);
      block->instrs().push_back(new CallInstruction(lv));
   }


   void emit_frameset(CgenEnv& env, Block *block) {
      /* push ix
       * ld ix,-<frame>
       * add ix,sp
       * ld sp,ix
       */
      const FrameValue *bytes = env.ext_env().frame().neg_callee_bytes();
      Instructions instrs
         {new PushInstruction(&rv_ix),
          new LoadInstruction(&rv_ix, bytes),
          new AddInstruction(&rv_ix, &rv_sp),
          new LoadInstruction(&rv_sp, &rv_ix)
         };
      block->instrs().insert(block->instrs().begin(), instrs.begin(), instrs.end());
   }
   
   void emit_frameunset(CgenEnv& env, Block *block) {
      /* lea ix,ix+locals_bytes
       * ld sp,ix
       * pop ix
       * ret
       */
      block->instrs().push_back(new LeaInstruction
                                (&rv_ix, new IndexedRegisterValue
                                 (&rv_ix,
                                  IndexedRegisterValue::Index(env.ext_env().frame().saved_fp()))));
      block->instrs().push_back(new LoadInstruction(&rv_sp, &rv_ix));
      block->instrs().push_back(new PopInstruction(&rv_ix));
      block->instrs().push_back(new RetInstruction());
   }

   void emit_invert_flag(Block *block, const FlagValue *flag) {
      switch (flag->cond_0()) {
      case Cond::Z:
      case Cond::NZ:
         /* ld a,0
          * jr nz,_
          * inc a
          * _
          * or a,a
          * ------ 6
          */
         block->instrs().push_back(new LoadInstruction(&rv_a, &imm_b<0>));
         block->instrs().push_back(new JrInstruction(&imm_b<3>, Cond::NZ));
         block->instrs().push_back(new IncInstruction(&rv_a));
         block->instrs().push_back(new OrInstruction(&rv_a, &rv_a));
         break;
         
      case Cond::C:
      case Cond::NC:
         /* ccf */
         block->instrs().push_back(new CcfInstruction());
         break;

      case Cond::P:
      case Cond::M:
         emit_crt("___sfinv", block);
         break;
         
      case Cond::ANY: abort();
      }
   }

    void emit_convert_flag(Block *block, const FlagValue *from, const FlagValue *to) {
        if (from->Eq(to)) { return; }

        if (from->Eq(to->invert())) {
            emit_invert_flag(block, from);
            return;
        }

        switch (from->cond_0()) {
        case Cond::C:
        case Cond::NC:
            switch (to->cond_0()) {
            case Cond::Z:
            case Cond::NZ:
            case Cond::P:
            case Cond::M:
                /* CF -> ZF|SF
                 * [ccf]
                 * sbc a,a
                 */
                {
                    bool ccf = ((from->cond_1() == Cond::C) ==
                                (to->cond_1() == Cond::Z || to->cond_1() == Cond::P));
                    if (ccf) {
                        block->instrs().push_back(new CcfInstruction());
                    }
                    block->instrs().push_back(new SbcInstruction(&rv_a, &rv_a));
                }
                break;

            case Cond::C:
            case Cond::NC:
            case Cond::ANY:
               abort();
            }
              
        case Cond::Z:
        case Cond::NZ:
            switch (to->cond_0()) {
            case Cond::C:
            case Cond::NC:
                /* ZF -> CF
                 * scf
                 * jr z/nz,_ ; 
                 * ccf
                 * _      
                 */
                {
                    Cond cc = ((from->cond_0() == Cond::Z) == (to->cond_0() == Cond::C)) ?
                        Cond::NZ : Cond::Z;
                    block->instrs().push_back(new ScfInstruction());
                    block->instrs().push_back(new JrInstruction(&imm_b<3>, cc));
                    block->instrs().push_back(new CcfInstruction());
                }
                break;

            case Cond::M:
            case Cond::P:
                /* ZF -> SF
                 * ld a,0
                 * jr z/nz,_
                 * dec a
                 * _
                 * or a,a
                 * ---------
                 * NOTE: ZF and SF are always modified together as far as I can tell,
                 *       so Z => P (or ZF => ~SF). Converse/inverse does not hold.
                 */
                {
                    Cond cc = ((from->cond_1() == Cond::Z) == (to->cond_1() == Cond::P)) ?
                        Cond::Z : Cond::NZ;
                    block->instrs().push_back(new LoadInstruction(&rv_a, &imm_b<0>));
                    block->instrs().push_back(new JrInstruction(&imm_b<3>, cc));
                    block->instrs().push_back(new OrInstruction(&rv_a, &rv_a));
                }
                break;

            case Cond::Z:
            case Cond::NZ:
            case Cond::ANY:
               abort();
            }
            break;
          
        case Cond::P:
        case Cond::M:
            switch (to->cond_0()) {
            case Cond::C:
            case Cond::NC:
                /* SF -> CF
                 * call ___sftocf[n]
                 */
               {
                  const char *crt = ((from->cond_1() == Cond::M) == (to->cond_1() == Cond::C)) ?
                     "___sftocf" : "___sftocfn";
                  emit_crt(crt, block);
               }
               break;
                
            case Cond::Z:
            case Cond::NZ:
                /* SF -> ZF
                 * call ___sftozf
                 */
               {
                  const char *crt = ((from->cond_1() == Cond::M) == (to->cond_1() == Cond::Z)) ?
                     "___sftozf" : "___sftozfn";
                  emit_crt(crt, block);
               }
               break;

            default: abort();
            }
            
        default: abort();
        }
    }

}
