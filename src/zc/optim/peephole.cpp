#include <list>

#include "peephole.hpp"
#include "asm.hpp"
#include "optim.hpp"

namespace zc::z80 {

   void PeepholeOptimization::ReplaceAll(Instructions& input) {
      for (const_iterator it = input.begin(), end = input.end(); it != end; ) {
         it = ReplaceAt(input, it);
      }
   }

   Instructions::const_iterator PeepholeOptimization::ReplaceAt(Instructions& input,
                                                                const_iterator it) {
      Instructions new_instrs;
      const_iterator rm_end = replace_(it, input.end(), new_instrs);
      
      if (rm_end != it) {
         ++hits_;
      }
      ++total_;
      
      const_iterator ret_it;
      const_iterator new_end = input.erase(it, rm_end);
      input.splice(new_end, new_instrs);
      if (it == rm_end) {
         ++new_end;
      }
      return new_end;
   }

   void PeepholeOptimization::PassBlock(Block *block, PeepholeOptimization *optim) {
      Instructions& instrs = block->instrs();
      optim->ReplaceAll(instrs);
   }

   void PeepholeOptimization::Pass(FunctionImpl *impl) {
      Blocks visited;
      void (*fn)(Block *block, PeepholeOptimization *optim) =
         PeepholeOptimization::PassBlock;
      impl->entry()->for_each_block(visited, fn, this);
      impl->fin()->for_each_block(visited, fn, this);
   }

   void PeepholeOptimization::Dump(std::ostream& os) const {
      os << name_ << "\t" << hits_ << "\t" << total_ << "\t" << bytes_saved_ * hits_;
   }

   /*** PEEPHOLE OPTIMIZATION FUNCTIONS ***/

   /* Indexed Register Load/Store
    * lea rr1,ix+*
    * ld (rr1),v | ld r2,(rr1)
    * ------------
    * ld (ix+*),v | ld r2,(ix+*)
    */
   Instructions::const_iterator peephole_indexed_load_store(Instructions::const_iterator begin,
                                                            Instructions::const_iterator end,
                                                            Instructions& out) {
      Instructions::const_iterator it = begin;

      const auto no_match = [&](){ out.clear(); return begin; };

      /* unbound values */
      const Register *rr1;
      const Register *rr2;
      IndexedRegisterValue::Index frame_index;
      
      /* instruction 1: lea rr1,ix+* */
      if (it == end) { return no_match(); }
      const RegisterValue rr1_1(&rr1, long_size);
      const IndexedRegisterValue idx_(&rv_ix, &frame_index);
      const LeaInstruction instr1(&rr1_1, &idx_);
      if (!instr1.Match(*it)) { return no_match(); }
      ++it;
      
      /* instruction 3: ld (rr1),rr2 | ld rr2,(rr1) */
      if (it == end) { return no_match(); }
      const RegisterValue rr1_3(rr1);
      const MemoryValue rr1_v_3(&rr1_3, long_size);
      int rr2_size;
      const RegisterValue rr2_3(&rr2, &rr2_size);
      const LoadInstruction instr3a(&rr1_v_3, &rr2_3);
      const LoadInstruction instr3b(&rr2_3, &rr1_v_3);
      const MemoryValue *memval = new MemoryValue
         (new IndexedRegisterValue(&rv_ix, frame_index), &rr2_size);
      const Value *dst, *src;
      if (instr3a.Match(*it)) {
         /* instruction 3a: ld (rr1),rr2 */
         dst = memval;
         src = new RegisterValue(rr2, rr2_size);
      } else if (instr3b.Match(*it)) {
         /* instruction 3b: ld rr2,(rr1) */
         dst = new RegisterValue(rr2, rr2_size);
         src = memval;
      } else {
         return no_match();
      }
      ++it;
      
      /* generate replacement */
      out.push_back(new LoadInstruction(dst, src));
      
      return it;
   }

   Instructions::const_iterator peephole_push_pop(Instructions::const_iterator begin,
                                                  Instructions::const_iterator end,
                                                  Instructions& out) {
      /* push rr1
       * pop rr2
       */
      Instructions::const_iterator it = begin;
      const auto no_match = [&](){ out.clear(); return begin; };

      /* unbound values */
      const Register *rr1;
      const Register *rr2;

      /* instruction 1: push rr1 */
      if (it == end) { return no_match(); }
      const RegisterValue rr1_1(&rr1, long_size);
      const PushInstruction instr1(&rr1_1);
      if (!instr1.Match(*it)) { return no_match(); }
      ++it;

      /* instruction 2: pop rr2 */
      if (it == end) { return no_match(); }
      const RegisterValue rr2_2(&rr2, long_size);
      const PopInstruction instr2(&rr2_2);
      if (!instr2.Match(*it)) { return no_match(); }
      ++it;

      /* if rr1 == rr2, delete matched sequence */
      if (rr1->Eq(rr2)) { return it; }

      /* if rr1 == de and rr2 == hl or vice versa, replace with `ex de,hl` */
      if ((rr1->Eq(&r_de) && rr2->Eq(&r_hl)) ||
          (rr1->Eq(&r_hl) && rr2->Eq(&r_de))) {
         out.push_back(new ExInstruction(&rv_de, &rv_hl));
         return it;
      }

      return no_match();
   }

   Instructions::const_iterator peephole_pea(Instructions::const_iterator begin,
                                             Instructions::const_iterator end,
                                             Instructions& out) {
      /* lea rr,ix+*
       * push rr
       */
      Instructions::const_iterator it = begin;
      const auto no_match = [&](){ out.clear(); return begin; };

      /* unbound values */
      const Register *rr;
      IndexedRegisterValue::Index index;

      /* instruction 1 */
      if (it == end) { return no_match(); }
      const RegisterValue rv_1(&rr, long_size);
      const IndexedRegisterValue ir_1(&rv_ix, &index);
      const LeaInstruction instr1(&rv_1, &ir_1);
      if (!instr1.Match(*it)) { return no_match(); }
      ++it;

      /* instruction 2 */
      if (it == end) { return no_match(); }      
      const RegisterValue rv_2(rr);
      const PushInstruction instr2(&rv_2);
      if (!instr2.Match(*it)) { return no_match(); }
      ++it;

      /* replace */
      out.push_back(new PeaInstruction(new IndexedRegisterValue(&rv_ix, index)));
      return it;
   }

   Instructions::const_iterator peephole_self_load(Instructions::const_iterator begin,
                                                   Instructions::const_iterator end,
                                                   Instructions& out) {
      /* ld r,r */      
      Instructions::const_iterator it = begin;
      const auto no_match = [&](){ out.clear(); return begin; };      

      /* unbound values */
      const Register *r1, *r2;
      int size;
      
      if (it == end) { return no_match(); }
      const RegisterValue rv1(&r1, &size);
      const RegisterValue rv2(&r2, &size);
      const LoadInstruction instr(&rv1, &rv2);
      if (!instr.Match(*it)) { return no_match(); }
      if (!r1->Eq(r2)) { return no_match(); }
      ++it;

      return it;
   }

   Instructions::const_iterator peephole_frameset_0(Instructions::const_iterator begin,
                                                    Instructions::const_iterator end,
                                                    Instructions& out) {
      /* ld ix,0
       * add ix,sp
       * ld sp,ix
       * ---------
       * ld ix,0
       * add ix,sp
       */
      Instructions::const_iterator it = begin;
      const auto no_match = [&]() { out.clear(); return begin; };

      int size;
      const LoadInstruction instr1(&rv_ix, new ImmediateValue((intmax_t) 0, &size));
      const AddInstruction instr2(&rv_ix, &rv_sp);
      const LoadInstruction instr3(&rv_sp, &rv_ix);

      for (const Instruction *instr : {(const Instruction *) &instr1,
                                          (const Instruction *) &instr2,
                                          (const Instruction *) &instr3}) {
         if (it == end) { return no_match(); }
         if (!instr->Match(*it)) { return no_match(); }
         out.push_back(*it);
         ++it;
      }

      out.pop_back();
      
      return it;
   }


   Instructions::const_iterator peephole_frameunset_0(Instructions::const_iterator begin,
                                                      Instructions::const_iterator end,
                                                      Instructions& out) {
      /* lea ix,ix+0
       * ld sp,ix
       */
      Instructions::const_iterator it = begin;
      const auto no_match = [&]() { out.clear(); return begin; };

      const IndexedRegisterValue idx(&rv_ix, 0);
      const LeaInstruction instr1(&rv_ix, &idx);
      const LoadInstruction instr2(&rv_sp, &rv_ix);

      if (it == end) { return no_match(); }
      if (!instr1.Match(*it)) { return no_match(); }
      ++it;
      if (it == end) { return no_match(); }
      if (!instr2.Match(*it)) { return no_match(); }
      ++it;

      return it;
   }
   
   
   Instructions::const_iterator peephole_lea_nop(Instructions::const_iterator begin,
                                                 Instructions::const_iterator end,
                                                 Instructions& out) {
      /* lea xy,xy+0
       */
      Instructions::const_iterator it = begin;
      const auto no_match = [&]() { out.clear(); return begin; };

      const Register *xy1, *xy2;
      const RegisterValue xyv1(&xy1, long_size);
      const RegisterValue xyv2(&xy2, long_size);
      const IndexedRegisterValue idx(&xyv2, 0);
      const LeaInstruction instr(&xyv1, &idx);

      if (it == end) { return no_match(); }
      if (!instr.Match(*it)) { return no_match(); }
      if (!xy1->Eq(xy2)) { return no_match(); }

      ++it;

      return it;
   }
   
   std::forward_list<PeepholeOptimization> peephole_optims = 
      {PeepholeOptimization("indexed-load", peephole_indexed_load_store, 2),
       PeepholeOptimization("push-pop", peephole_push_pop, 2), 
       PeepholeOptimization("pea", peephole_pea, 1),
       PeepholeOptimization("self-load", peephole_self_load, 1),
       PeepholeOptimization("frameset-0", peephole_frameset_0, 2),
       PeepholeOptimization("frameunset-0", peephole_frameunset_0, 5),
       PeepholeOptimization("lea-nop", peephole_lea_nop, 3),
      };

}
