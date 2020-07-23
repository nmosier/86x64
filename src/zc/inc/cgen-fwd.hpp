#ifndef __CGEN_FWD_HPP
#define __CGEN_FWD_HPP

#include <unordered_set>

namespace zc {

   class Block;
   typedef std::unordered_set<Block *> Blocks;
   class StatInfo;
   class SymInfo;
   class StackFrame;
   class CgenExtEnv;
   class BlockTransition;
   class BlockTransitions;
   class FunctionImpl;
   class FunctionImpls;
   class StringConstants;
   class CgenEnv;
   enum class Cond;
   Cond invert(Cond cond);
    
   
}

#endif
