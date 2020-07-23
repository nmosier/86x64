#include <array>
#include <ostream>

#include "asm.hpp"

namespace zc::z80 {

   Flag ZF(std::array {"nz", "z"}),
      CF(std::array {"nc", "c"});
   
   void Flag::Emit(std::ostream& os, bool cond) const {
      os << (cond ? std::get<1>(name()) : std::get<0>(name()));
   }

   void FlagState::Emit(std::ostream& os) const {
      flag()->Emit(os, state());
   }
   
}
