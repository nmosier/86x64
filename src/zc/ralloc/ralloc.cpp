#include <cassert>
#include <list>
#include <unordered_map>
#include <map>

#include "ralloc.hpp"
#include "cgen.hpp"
#include "optim.hpp"

namespace zc::z80 {

   
   bool before(Instructions::const_iterator lhs, Instructions::const_iterator rhs,
               Instructions::const_iterator end) {
      while (lhs != end && lhs != rhs) {
         ++lhs;
      }
      
      return lhs == rhs; /* NOTE: This also works if rhs == end. */
   }


   bool RallocInterval::in(const RallocInterval& other) const {
      auto other_begin = other.begin;
      while (other_begin != other.end && other_begin != begin) {
         ++other_begin;
      }
      if (other_begin != begin) {
         return false;
      }

      auto other_end = other.end;
      while (other_end != other.begin && other_end != end) {
         --other_end;
      }
      if (other_end != end) {
         return false;
      }
      
      return true;
   }

   bool RallocInterval::intersects(const RallocInterval& other) const {
      if (in(other) || other.in(*this)) {
         return false;
      }
      
      auto begin_ = begin;
      while (begin_ != end && begin_ != other.end) {
         ++begin_;
      }
      if (begin_ == other.end) {
         return true;
      }

      auto end_ = end;
      while (end_ != begin && end_ != other.begin) {
         --end_;
      }
      if (end_ == other.begin) {
         return true;
      }

      return false;
   }

   bool RallocInterval::operator<(const RallocInterval& other) const {
      /* first sort by length */
      return length() < other.length();
   }

   
      enum class mode {GEN, USE};
      template <typename Key>
      using IntervalMap = std::map<Key, std::vector<std::pair<Instructions::iterator,mode>>>;

   void RegisterAllocator::ComputeIntervals() {

#if 1
      alg::ValueSet gens, uses;
#else
      std::list<const Value *> gens, uses;
#endif

      IntervalMap<const ByteRegister *> byte_regs;
      IntervalMap<const VariableValue *> vars;

      /* populate local regs maps with all alloc'able regs */
      for (const ByteRegister *reg : {&r_a, &r_b, &r_c, &r_d, &r_e, &r_h, &r_l}) {
         byte_regs[reg];
      }

      /* iterate thru instructions */
      int instr_index = 0;
      for (auto instr_it = block()->instrs().begin();
           instr_it != block()->instrs().end();
           ++instr_it, ++instr_index, gens.clear(), uses.clear()) {
         /* get gens and uses in instrution */
         (*instr_it)->Kill(std::inserter(gens, gens.begin()));
         (*instr_it)->Gen(std::inserter(uses, uses.begin()));

         auto fn =
            [&](mode m)
            {
               for (const Value *val : (m == mode::GEN) ? gens : uses) {
                  const Register *reg = val->reg();
                  const VariableValue *var = dynamic_cast<const VariableValue *>(val);
                  
                  /* add register */
                  if (reg) {
                     /* process byte reg lifetimes */
                     std::list<const ByteRegister *> cur_regs;
                     if (reg->kind() == Register::Kind::REG_MULTIBYTE) {
                        auto arr = dynamic_cast<const MultibyteRegister *>(reg)->regs();
                        cur_regs.insert(cur_regs.end(), arr.begin(), arr.end());
                     } else {
                        cur_regs = {dynamic_cast<const ByteRegister *>(reg)};
                     }
                     for (const ByteRegister *byte_reg : cur_regs) {
                        auto it = byte_regs.find(byte_reg);
                        if (it != byte_regs.end()) {
                           it->second.push_back({instr_it, m});
                           // it->second.insert({instr_it, m});
                        }
                     }
                  }

                  /* add variable */
                  if (var) {
                     vars[var].push_back({instr_it, m});
                  }
               }
            };

         fn(mode::USE);
         fn(mode::GEN);
      }

      /* convert results to intervals */

      /* compute register free intervals, backwards */
      for (auto reg_it : byte_regs) {
         RallocIntervals reg_free_ints;
         auto info_it = reg_it.second.rbegin();
         auto info_end = reg_it.second.rend();
         RallocInterval cur_int;

         /* loop invariant: end set */
         cur_int.end = --block()->instrs().end();
         do {
            /* Find 1st `use'. */
#if 1
            for (; info_it != info_end && info_it->second != mode::USE; ++info_it) {
               assert(info_it->second == mode::GEN);
               cur_int.begin = info_it->first;
               reg_free_ints.push_back(cur_int);
               cur_int.end = info_it->first;
            }
#else
            while (info_it != info_end && info_it->second != mode::USE) {
               ++info_it;
            }
#endif
            
            if (info_it == info_end) {
               // cur_int.begin = 0;
               cur_int.begin = block()->instrs().begin();
            } else {
               // cur_int.begin = info_it->first;
               auto tmp_it = info_it->first;
               cur_int.begin = tmp_it;
            }

            if (before(cur_int.begin, cur_int.end, block()->instrs().end())) {
               // if (cur_int.begin <= cur_int.end) {
               reg_free_ints.push_back(cur_int);
            }

            /* skip until `gen' */
            while (info_it != info_end && info_it->second != mode::GEN) {
               ++info_it;
            }
            if (info_it != info_end) {
               cur_int.end = (Instructions::iterator) info_it->first;
            }
         } while (info_it != info_end);

         /* insert into ralloc list */
         regs_.insert({reg_it.first, RegisterFreeIntervals(reg_free_ints)});
      }      

      /* process variable lifetime intervals */
      for (auto var_it : vars) {
         auto info_it = var_it.second.begin();
         auto info_end = var_it.second.end();
         assert(info_it->second == mode::GEN);

         VariableRallocInfo info(var_it.first, info_it->first);

         /* add uses */
         ++info_it;
         while (info_it != info_end) {
            info.uses.push_back(info_it->first);
            info.interval.end = info_it->first;

            ++info_it;
         }

         /* add to vars_ */
         vars_.insert({var_it.first->id(), info});
      }
      
   }

   void RegisterAllocator::GetAssignableRegs(const VariableValue *var,
                                             std::unordered_set<const Register *>& out) const {
      /* get variable lifetime */
      const VariableRallocInfo& varinfo = vars_.at(var->id());
      const RallocInterval& varint = varinfo.interval;
         
      /* look thru byte regs to find free */
      std::unordered_set<const ByteRegister *> byte_regs_;
      for (auto reg_it : regs_) {
         auto super_it = reg_it.second.superinterval(varint);
         if (super_it != reg_it.second.intervals.end()) {
            byte_regs_.insert(reg_it.first);
         }
      }

      /* if multibyte var, find register pairs in available byte regs */
      switch (var->size()) {
      case byte_size:
         for (auto byte_reg : byte_regs_) {
            out.insert(byte_reg);
            // *out++ = byte_reg;
         }
         break;
            
      case long_size:
         for (const MultibyteRegister *multibyte_reg : {&r_bc, &r_de, &r_hl}) {
            auto subregs = multibyte_reg->regs();
            if (std::all_of(subregs.begin(), subregs.end(),
                            [&](const ByteRegister *byte_reg) -> bool {
                               return byte_regs_.find(byte_reg) != byte_regs_.end();
                            })) {
               out.insert(multibyte_reg);
               // *out++ = multibyte_reg;
            }
         }
         break;
            
      default: abort();
      }
   }

   bool RegisterAllocator::TryRegAllocVar(const VariableValue *var) {
      assert(var->requires_alloc());

      /* get candidate registers */
      std::unordered_set<const Register *> candidate_regs;      
      GetAssignableRegs(var, candidate_regs);
      if (candidate_regs.empty()) { return false; }

      /* determine nearby regs */
      VariableRallocInfo& var_info = vars_.at(var->id());
      std::unordered_map<const Register *, int> nearby_regs;     
      
      const RegisterValue *var_src = dynamic_cast<const RegisterValue *>((*var_info.gen)->src());
      if (var_src) {
         nearby_regs[var_src->reg()] += 1;
      }

      for (auto use : var_info.uses) {
         auto var_dst = dynamic_cast<const RegisterValue *>((*use)->dst());
         if (var_dst) {
            nearby_regs[var_dst->reg()] += 1;
         }
      }

      /* remove unfree regs */
      for (auto it = nearby_regs.begin(), end = nearby_regs.end(); it != end; ) {
         if (candidate_regs.find(it->first) == candidate_regs.end()) {
            it = nearby_regs.erase(it);
         } else {
            ++it;
         }
      }

      const Register *reg;
      
      /* find ``nearest'' reg */
      if (nearby_regs.empty()) {
         /* IMPROVE -- might need to pick better */
         reg = *candidate_regs.begin(); // NOTE: guaranteed at this point to be at least one.
      } else {
         reg = std::max_element(nearby_regs.begin(), nearby_regs.end(),
                                [](const auto acc, const auto next) -> bool {
                                   return acc.second > next.second;
                                })->first;
      }
      
      /* allocate reg to var */
      var_info.AssignVal(new RegisterValue(reg));
      
      /* remove register free intervals */
      switch (reg->kind()) {
      case Register::Kind::REG_BYTE:
         regs_.at(dynamic_cast<const ByteRegister *>(reg)).
            remove_interval(var_info.interval);
         break;
         
      case Register::Kind::REG_MULTIBYTE:
         {
            auto multibyte_reg = dynamic_cast<const MultibyteRegister *>(reg);
            for (auto byte_reg : multibyte_reg->regs()) {
               regs_.at(byte_reg).remove_interval(var_info.interval);
            }
         }
         break;
      default: abort();
      }

      return true;
   }
   
   AllocKind RegisterAllocator::AllocateVar(const VariableValue *var) {
      assert(var->requires_alloc());
      
      VariableRallocInfo& var_info = vars_.at(var->id());      
      bool has_reg = TryRegAllocVar(var);
      if (has_reg) {
         var_info.alloc_kind = AllocKind::ALLOC_REG;
         return AllocKind::ALLOC_REG;
      }
      if (var_info.requires_reg()) {
         throw std::logic_error("could not allocate register to variable that requires register");
      }

      /* check whether stack-spillable */
      if (var_info.is_stack_spillable()) {
         /* try to stack-spill */
         if (stack_spills_.try_add(var_info.interval)) {
            var_info.StackSpill(block()->instrs());
            return AllocKind::ALLOC_STACK;
         }
      }

      /* otherwise, frame-spill */
      var_info.FrameSpill(stack_frame_);

      return var_info.alloc_kind;
   }

   bool VariableRallocInfo::requires_reg() const {
      if (var->force_reg()) { return true; }
      if (var->size() == byte_size) { return false; }

      /* check if generated from immediate */
      intmax_t imm;
      const ImmediateValue iv(&imm, var->size());
      const LoadInstruction gen_instr(var, &iv);
      if (gen_instr.Match(*gen)) { return true; }

      /* check if stored to memory value */
      const Value *addr;
      const MemoryValue mv(&addr, var->size());
      const LoadInstruction use_instr(&mv, var);
      if (std::any_of(uses.begin(), uses.end(),
                  [&](auto it) -> bool {
                     return use_instr.Match(*it);
                  })) {
         return true;
      }
      
      return false; // might need to check more things 
   }

   bool VariableRallocInfo::is_stack_spillable() const {
      if (var->size() == byte_size) { return false; } /* must be word/long to be spilled */

      /* verify that gen instruction can be translated into `push' */
      const Register *reg_ptr;
      const RegisterValue reg_val(&reg_ptr, var->size());
      const LoadInstruction gen_instr(var, &reg_val);
      if (!gen_instr.Match(*gen)) { return false; }

      /* verify that each use can be translated into `pop' into register */
      if (!std::all_of(uses.begin(), uses.end(),
                       [&](auto use) -> bool {
                          const Register *reg_ptr;
                          const RegisterValue reg_val(&reg_ptr, var->size());
                          const LoadInstruction use_instr(&reg_val, var);
                          return use_instr.Match(*use);
                       })) {
         return false;
      }
      
      return true;
   }

   void VariableRallocInfo::StackSpill(Instructions& instrs) {
      *gen = new PushInstruction((*gen)->src());
      int i = uses.size();
      for (Instructions::iterator use : uses) {
         auto rv = (*use)->dst();
         *use++ = new PopInstruction(rv);
         if (i > 1) {
            instrs.insert(use, new PushInstruction(rv));
         }
         
         --i;
      }
      alloc_kind = AllocKind::ALLOC_STACK;
   }

   void VariableRallocInfo::FrameSpill(StackFrame& frame) {
      auto frame_val = frame.next_tmp(var);
      AssignVal(frame_val);
      alloc_kind = AllocKind::ALLOC_FRAME;
   }
   
   std::ostream& operator<<(std::ostream& os, AllocKind kind) {
      switch (kind) {
      case AllocKind::ALLOC_NONE: os << "ALLOC_NONE"; return os;
      case AllocKind::ALLOC_REG: os << "ALLOC_REG"; return os;
      case AllocKind::ALLOC_STACK: os << "ALLOC_STACK"; return os;
      case AllocKind::ALLOC_FRAME: os << "ALLOC_FRAME"; return os;
      }
   }
   
   void RegisterAllocator::RunAllocation() {
      /* 0. Allocate regs to all variables that require a register. 
       * 1. Go thru remaining regs and allocate in decreasing order of USES / LIFETIME ratio.
       */
      std::vector<Vars::iterator> prioritized_vars;
      for (auto it = vars_.begin(); it != vars_.end(); ++it) {
         if (it->second.var->requires_alloc()) {
            prioritized_vars.push_back(it);
         }
      }
      std::sort(prioritized_vars.begin(), prioritized_vars.end(),
                [](auto a, auto b) { return a->second.priority() > b->second.priority(); });
      
      for (auto it : prioritized_vars) {
         AllocateVar(it->second.var);
      }
   }

   RallocIntervals::iterator RegisterFreeIntervals::superinterval(const RallocInterval& interval) {
      auto it = intervals.begin(), end = intervals.end();      
      for (; it != end; ++it) {
         if (interval.in(*it)) {
            break;
         }
      }
      return it;
   }

   void RegisterFreeIntervals::remove_interval(const RallocInterval& interval) {
      auto it = superinterval(interval);
      if (it == intervals.end()) { throw std::logic_error("asked to remove interval not present"); }
      intervals.emplace_back(it->begin, interval.begin);
      intervals.emplace_back(interval.end, it->end);
      intervals.erase(it);
   }

   void VariableRallocInfo::AssignVal(const Value *newval) {
      (*gen)->ReplaceVar(var, newval);
      for (auto use : uses) {
         (*use)->ReplaceVar(var, newval);
      }

      allocated_val = newval;
   }


   /*** DUMPS ***/
   void RegisterFreeIntervals::Dump(std::ostream& os, Instructions::iterator instrs_begin)
      const {
      for (const RallocInterval& interval : intervals) {
         interval.Dump(os, instrs_begin);
         os << ","; 
      }
      os << std::endl;
   }

   void VariableRallocInfo::Dump(std::ostream& os, Instructions::iterator instrs_begin) const {
      var->Emit(os);
      os << " " << alloc_kind;
      if (allocated_val) { os << " "; allocated_val->Emit(os); }
      os << std::endl;
      os << "\tinterval:\t";
      interval.Dump(os, instrs_begin);
      os << std::endl;
      os << "\tgen:\t";
      (*gen)->Emit(os); 
      for (auto it : uses) {
         os << "\tuse:\t";
         (*it)->Emit(os);
      }
   }

   void RegisterAllocator::Dump(std::ostream& os) const {
      for (auto it : regs_) {
         it.first->Dump(os);
         os << ": ";
         for (auto interval : it.second.intervals) {
            interval.Dump(os, block()->instrs().begin());
            os << " ";
         }
         os << std::endl;
      }

      for (auto it : vars_) {
         it.second.Dump(os, block()->instrs().begin());
      }
   }

   /*** ***/
   void RegisterAllocator::RallocBlock(Block *block, StackFrame& stack_frame) {
      RegisterAllocator ralloc(block, stack_frame);
      ralloc.ComputeIntervals();

      if (g_optim.join_vars) {
         ralloc.JoinVars();
      }

      if (g_print.ralloc_info) {
         std::cerr << block->label()->name() << ":" << std::endl;
         ralloc.Dump(std::cerr);
      }

      ralloc.RunAllocation();

      if (g_print.ralloc_info) {
         ralloc.Dump(std::cerr);
      }
   }

   void RegisterAllocator::Ralloc(FunctionImpl& impl, StackFrame& frame) {
      Blocks visited;
      void (*fn)(Block *, StackFrame&) = RegisterAllocator::RallocBlock;
      impl.entry()->for_each_block(visited, fn, frame);
      impl.fin()->for_each_block(visited, fn, frame);
   }

   void RegisterAllocator::Ralloc(CgenEnv& env) {
      for (FunctionImpl& impl : env.impls().impls()) {
         Ralloc(impl, env.ext_env().frame());
      }
   }

   /*** ***/
   bool NestedRallocIntervals::try_add(const RallocInterval& interval) {
      for (auto nested_interval : intervals_) {
         if (interval.intersects(nested_interval) && !interval.in(nested_interval) &&
             nested_interval.in(interval)) {
            /* overlaps */
            return false;
         }
      }
      
      intervals_.push_back(interval);
      return true;
   }

   const VariableValue *VariableRallocInfo::joinable() {
      /* Requirements for joining:
       *  - Variable must have exactly one use. 
       *  - Gen instruction must be a `ld`.
       *  - Variables must have compatible traits (e.g. both must require registers or not).
       */

      if (uses.size() != 1) { return nullptr; }
      Instructions::iterator use = uses.front();

      const Value *other_val;
      const LoadInstruction ld_instr(&other_val, var);
      if (!ld_instr.Match(*use)) { return nullptr; }
      const VariableValue *other_var = dynamic_cast<const VariableValue *>(other_val);
      if (other_var == nullptr) { return nullptr; }

      if (!var->Compat(other_var)) { return nullptr; }

      return other_var;
   }

   void RegisterAllocator::JoinVars() {
      for (auto pair : vars_) {
         auto var_info = pair.second;
         auto second_var = var_info.joinable();
         if (second_var) {
            JoinVar(var_info.var, second_var);
         }
      }
   }

   void RegisterAllocator::JoinVar(const VariableValue *first, const VariableValue *second) {
      /* find associated infos */
      auto first_info = vars_.at(first->id());
      auto second_info = vars_.at(second->id());

      /* Join variables:
       *  - Merge lifetime intervals into one
       *  - Copy other variable's uses into this variable's uses.
       *  - Replace all references to other variable.
       *  - Delete other variable's `gen` instruction.
       *  - Unmap second variable from `vars_`.
       */

      /* merge lifetime intervals */
      first_info.interval.Merge(second_info.interval);

      /* copy _second_'s uses into this var's uses */
      assert(first_info.uses.size() == 1);
      first_info.uses = second_info.uses;

      /* replace all references to other variable */
      (*second_info.gen)->ReplaceVar(second, first);
      for (auto instr_it : second_info.uses) {
         (*instr_it)->ReplaceVar(second, first);
      }

      /* delete _second_ variable's `gen` instruction */
      block_->instrs().erase(second_info.gen);

      /* unmap variable from `vars_` */
      vars_.erase(second->id());
   }
         
   void RallocInterval::Merge(const RallocInterval& with, RallocInterval& out) const {
      if (end == with.begin) {
         out.begin = begin;
         out.end = with.end;
      } else if (begin == with.end) {
         out.begin = with.begin;
         out.end = end;
      } else {
         throw std::logic_error("attempt to merge non-incident intervals");
      }
   }
   
}
