#ifndef __ENV_HPP
#define __ENV_HPP

#include <set>
#include <forward_list>

#include "scopedtab.hpp"
#include "symtab.hpp"

namespace zc {

   class ASTType;
   class SemantError;
   
   /**
    * Class representing basic single-symbol environment.
    */
   class SymbolEnv {
   public:
      Symbol *sym() const { return sym_; }
      
      SymbolEnv(): sym_(nullptr) {}
      
      void Enter(Symbol *sym) {
         sym_ = sym;
      }
      
      void Exit() {
         sym_ = nullptr;
      }

      

   private:
      Symbol *sym_;
   };

   template <typename Value>
   class SearchableStack {
      typedef std::forward_list<Value *> Stack;
   public:

      /**
       * Push element onto statement stack.
       */
      void Push(Value *val) {
         return stack_.push_front(val);
      }

      /**
       * Pop element off statement stack.
       */
      void Pop() {
         stack_.pop_front();
      }

      /**
       * Get value at the top of the statement stack, leaving the stack unmodified.
       */
      Value *Peek() {
         if (stack_.size() == 0) {
            return nullptr;
         } else {
            return stack_.front();
         }
      }

      /**
       * @param pred a boolean predicate function that accepts exactly one parameter of StatValue
       * type.
       * @return value if found; nullptr if no value is found.
       */
      template <typename Pred>
      Value *Find(Pred pred) {
         for (auto val : stack_) {
            if (pred(val)) {
               return val;
            }
         }
         return nullptr;
      }
      
      
   private:
      Stack stack_;
   };
   
   /**
    * Base environment class used during semantic analysis and code generation.
    * @param SymtabValue value associated with symbol.
    * @param TaggedTypeValue value associated with tagged type.
    * @param StatValue value in statement stack.
    * @param ExtEnv external environment class.
    */
   template <typename SymtabValue, typename TaggedTypeValue, typename StatValue, class ExtEnv>
   class Env {
   public:
      typedef ScopedTable<Symbol *, SymtabValue> ScopedSymtab;
      typedef ScopedTable<Symbol *, TaggedTypeValue> ScopedTagtab;
      typedef SearchableStack<StatValue> StatStack;

      /* Accessors */
      ScopedSymtab& symtab() { return symtab_; }
      const ScopedSymtab& symtab() const { return symtab_; }
      
      ScopedTagtab& tagtab() { return tagtab_; }
      const ScopedTagtab& tagtab() const { return tagtab_; }
      
      ExtEnv& ext_env() { return ext_env_; }
      const ExtEnv& ext_env() const { return ext_env_; }

      StatStack& stat_stack() { return stat_stack_; }
      const StatStack& stat_stack() const { return stat_stack_; }

      /* Scoping */
      void EnterScope();
      void ExitScope();
      
   protected:
      ScopedSymtab symtab_;
      ScopedTagtab tagtab_;
      ExtEnv ext_env_;
      StatStack stat_stack_;
   };

}

#endif
