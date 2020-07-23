#include <cassert>

#include "asm.hpp"

namespace zc::z80 {

   void Label::EmitRef(std::ostream& os) const {
      os << name();
   }

   void Label::EmitDef(std::ostream& os) const {
      Label::EmitRef(os);
      os << ":" << std::endl;
   }

   Label *Label::Append(const std::string& suffix) const {
      return new Label(name() + suffix);
   }

   Label *Label::Prepend(const std::string& prefix) const {
      return new Label(prefix + name());
   }


}
