#ifndef __ASM_HPP
#error "include \"asm.hpp\""
#endif

#ifndef __ASM_LAB_HPP
#define __ASM_LAB_HPP

#include <string>
#include <vector>
#include <variant>

namespace zc::z80 {

   /**********
    * LABELS *
    **********/

   class Label {
   public:
      const std::string& name() const { return name_; }

      Label *Append(const std::string& suffix) const;
      Label *Prepend(const std::string& prefix) const;

      void EmitRef(std::ostream& os) const;
      void EmitDef(std::ostream& os) const;

      bool Eq(const Label *other) const { return name() == other->name(); }

      Label(const std::string& name): name_(name) {}

   protected:
      const std::string name_;
   };

}

#endif
