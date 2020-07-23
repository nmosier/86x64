#ifndef __ALG_HPP
#define __ALG_HPP

#include <unordered_set>
#include <iterator>

#include "asm-fwd.hpp" /* for VariableValue */
#include "cgen-fwd.hpp" /* for Cond */

namespace zc::alg {

   typedef std::unordered_set<const z80::Value *> ValueSet;
   typedef std::insert_iterator<ValueSet> ValueInserter;

   typedef std::unordered_set<const z80::VariableValue *> VariableSet;
   typedef std::insert_iterator<VariableSet> VariableInserter;
   
   typedef std::unordered_set<const z80::ByteRegister *> RegisterSet;
   typedef std::insert_iterator<RegisterSet> RegisterInserter;

   typedef std::unordered_set<Cond> CondSet;
   typedef std::insert_iterator<CondSet> CondInserter;

   extern const CondSet all_conds;
}

#endif
