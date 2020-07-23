#ifndef __EMIT_HPP
#define __EMIT_HPP

namespace zc {

   /**
    * Check whether expression is nonzero (i.e. evaluates to true as a predicate).
    * @param in value to test
    * @param output flags value
    */
   void emit_nonzero_test(CgenEnv& env, Block *block, const Value *in, const FlagValue **out);

   /**
    * Emit logical not on expression.
    */
   void emit_logical_not(CgenEnv& env, Block *block, const Value *in, const Value **out);

   void emit_booleanize_flag(CgenEnv& env, Block *block, const FlagValue *in, const Value **out,
                             int bytes);
   void emit_booleanize_flag_byte(CgenEnv& env, Block *block, const FlagValue *in,
                                  const Value **out);
   void emit_booleanize_flag_long(CgenEnv& env, Block *block, const FlagValue *in,
                                  const Value **out);
   
   
   /**
    * Emit code for predicate.
    */ 
   void emit_predicate(CgenEnv& env, Block *block, ASTExpr *expr, Block *take, Block *leave);  

   /**
    * Emit increment/decrement.
    * @param env codegen environment
    * @param block block to write instructions to
    * @param inc_not_dec whether to increment or decrement
    * @param pre_not_post whether output value should reflect update
    * @param subexpr subexpression to generate
    * @param out output value
    * @return continuation block
    */
   Block *emit_incdec(CgenEnv& env, Block *block, bool inc_not_dec, bool pre_not_post,
                      ASTExpr *subexpr, const Value **out);
   
   /** Killeric emission routine for performing binary operation on two integers. */
   Block *emit_binop(CgenEnv& env, Block *block, ASTBinaryExpr *expr,
                     const Value **out_lhs, const Value **out_rhs);

   /** Emit CRT frameset. */
   void emit_frameset(CgenEnv& env, Block *block);

   /** Emit CRT frameunset. */
   void emit_frameunset(CgenEnv& env, Block *block);

   /** Emit call to CRT routine. */
   void emit_crt(const std::string& name, Block *block);

   /** Emit asm to invert flag (CF or ZF). */
   void emit_invert_flag(Block *block, const FlagValue *flag);

   /** Emit asm to convert between flags (CF and ZF). Delegates to @see emit_convert_flag()
    * if possible.
    */
   void emit_convert_flag(Block *block, const FlagValue *from, const FlagValue *to);   


   
}

#endif
