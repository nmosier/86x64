#ifndef __RALLOC_HPP
#define __RALLOC_HPP

#include <unordered_map>
#include <unordered_set>
#include <list>
#include <ostream>
#include <iterator>

#include "asm.hpp"

namespace zc::z80 {
   struct RallocInterval;
}

namespace std {

}

namespace zc::z80 {

   /**
    * Check whether iterator comes before another iterator.
    */
   bool before(Instructions::const_iterator lhs, Instructions::const_iterator rhs,
               Instructions::const_iterator end);

   /**
    * Interval as used in representing a variable lifetime or when a register is free.
    */
   struct RallocInterval {
      Instructions::iterator begin;
      Instructions::iterator end;

      int length() const { return std::distance(begin, end); }

      bool operator<(const RallocInterval& other) const;

      bool in(const RallocInterval& other) const;

      /* NOTE: Checks for exclusive intersection, i.e. overlap without containment. */
      bool intersects(const RallocInterval& other) const;

      void Merge(const RallocInterval& with) { Merge(with, *this); }
      void Merge(const RallocInterval& with, RallocInterval& out) const;

      void Dump(std::ostream& os, Instructions::iterator instrs_begin) const {
         os << "[" << std::distance(instrs_begin, begin) << ","
            << std::distance(instrs_begin, end) << "]";
      }

      RallocInterval(Instructions::iterator begin, Instructions::iterator end):
         begin(begin), end(end) {}
         
      RallocInterval() {}
      
   };

#if 0
   class RallocIntervals {
   public:
      typedef std::multiset<RallocInterval>::iterator iterator;
      typedef std::multiset<RallocInterval>::const_iterator const_iterator;

      iterator begin() { return intervals_.begin(); }
      const_iterator begin() const { return intervals_.begin(); }
      iterator end() { return intervals_.end(); }
      const_iterator end() const { return intervals_.end(); }
      
   private:
      std::multiset<RallocInterval> intervals_;
   };
   
   typedef std::multiset<RallocInterval> RallocIntervals;
#else

   typedef std::list<RallocInterval> RallocIntervals;
      
#endif

   class NestedRallocIntervals {
   public:
      bool try_add(const RallocInterval& interval);
      
   protected:
      RallocIntervals intervals_;
   };

   enum class AllocKind {ALLOC_NONE, ALLOC_REG, ALLOC_STACK, ALLOC_FRAME};
   std::ostream& operator<<(std::ostream& os, AllocKind kind);
   
   /**
    * Variable info for the register allocator.
    */
   struct VariableRallocInfo {
      const VariableValue *var;
      const Value *allocated_val = nullptr;
      Instructions::iterator gen; /*!< where the variable is assigned (i.e. generated) */
      std::list<Instructions::iterator> uses; /*!< the instructions in which the variable is used */
      RallocInterval interval;
      AllocKind alloc_kind = AllocKind::ALLOC_NONE;

      void AssignVal(const Value *val);
      bool requires_reg() const;

      bool is_stack_spillable() const;
      void StackSpill(Instructions& instrs);

      void FrameSpill(StackFrame& frame); 

      /**
       * Check if variable is joinable with next variable.
       * @return variable to join; nullptr if no join was possible.
       */
      const VariableValue *joinable();
      
      void Dump(std::ostream& os, Instructions::iterator instrs_begin) const;

      VariableRallocInfo(const VariableValue *var, Instructions::iterator gen):
         var(var), gen(gen), interval(gen, gen) {}

      VariableRallocInfo(const VariableValue *var, Instructions::iterator gen,
                         const RallocInterval& interval):
         var(var), gen(gen), interval(interval) {}

      /**
       * Priority number when scheduling allocations. Higher number means higher priority.
       * Positive values for those that require registers; negative values for those that don't.xs
       */
      double priority() const {
         double base = (uses.size() + 1) / (interval.length() + 1);
         if (requires_reg()) { return base; }
         else { return -1/base; }
      }
   };

   /**
    * Register free intervals.
    */
   struct RegisterFreeIntervals {
      RallocIntervals intervals;

      RallocIntervals::iterator superinterval(const RallocInterval& subinterval);
      
      void remove_interval(const RallocInterval& interval);
      void Dump(std::ostream& os, Instructions::iterator instrs_begin) const;

      RegisterFreeIntervals(const RallocIntervals& intervals): intervals(intervals) {}
   };

   /**
    * Block register allocator.
    */
   class RegisterAllocator {
   public:
      Block *block() const { return block_; }

      void ComputeIntervals();
      void RunAllocation();

      void Dump(std::ostream& os) const;
      
      RegisterAllocator(Block *block, StackFrame& stack_frame):
         block_(block), stack_frame_(stack_frame) {}

      static void Ralloc(FunctionImpl& impl, StackFrame& stack_frame);
      static void Ralloc(CgenEnv& env);
      
   protected:
      Block *block_; /*!< block containing instructions. */
      StackFrame& stack_frame_; /*!< stack frame for spilling locals. */
      typedef std::unordered_map<int, VariableRallocInfo> Vars;
      Vars vars_; /*!< map from variable ID to register allocation information */
      typedef std::unordered_map<const ByteRegister *, RegisterFreeIntervals> Regs;
      Regs regs_;
      NestedRallocIntervals stack_spills_; /*!< intervals on which vars are stack-spilled */

      AllocKind AllocateVar(const VariableValue *var);

      /**
       * Try to allocate register to variable.
       * @param var variable to allocate to
       * @return whether allocation was successful
       */
      bool TryRegAllocVar(const VariableValue *var); 
      
      /**
       * Get registers that can be assigned to given variable.
       * @tparam output iterator to byte register
       * @param var variable
       * @param out output list of registers
       */
      void GetAssignableRegs(const VariableValue *var,
                             std::unordered_set<const Register *>& out) const;
      
      /**
       * Try to assign register to variable.
       * @param varinfo variable info
       * @return whether a register was assigned
       */
      bool TryAssignReg(VariableRallocInfo& varinfo);

      /**
       * Join variables.
       * @param first first variable to appear. The second variable will be joined into this one.
       * @param second second variable to appear. After joining, this one disappears.
       */
      void JoinVar(const VariableValue *first, const VariableValue *second);
      void JoinVars();

      static void RallocBlock(Block *block, StackFrame& stack_frame);
   };
   
}

#endif
