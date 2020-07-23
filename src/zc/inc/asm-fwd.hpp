#ifndef __ASM_FWD
#define __ASM_FWD

#include <vector>
#include <list>

#include "asm/asm-mode.hpp"
#include "util.hpp"

namespace zc::z80 {

   /* asm-val */
   class Value;
   typedef std::vector< portal<const Value *> > Values;
   class VariableValue;
   class FlagValue;
   class ImmediateValue;
   class LabelValue;
   class RegisterValue;
   class FrameValue;
   class OffsetValue;
   class MemoryValue;
   class ByteValue;

   /* asm-reg */
   class Register;
   class ByteRegister;
   class MultibyteRegister;
   
   /* asm-instr */
   class Instruction;
   typedef std::list<Instruction *> Instructions;   
   
   /* asm-cond */
   class Flag;
   class FlagState;
   enum class FlagMod;

   /* asm-crt */
   class CRT;

   /* asm-lab */
   class Label;
   
   
}

#endif
