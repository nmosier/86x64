#ifndef __ASM_HPP
#error "include \"asm.hpp\""
#endif

#ifndef __ASM_COND_HPP
#define __ASM_COND_HPP

#include <array>

namespace zc::z80 {

   class Flag {
   public:
      typedef std::array<const char *, 2> Name;
      const Name& name() const { return name_; }

      void Emit(std::ostream& os, bool cond) const;
      
      Flag(const Name& name): name_(name) {}
      
   protected:
      Name name_;
   };

   class FlagState {
   public:
      const Flag *flag() const { return flag_; }
      bool state() const { return state_; }

      void Emit(std::ostream& os) const;
      
      FlagState(const Flag *flag, bool state): flag_(flag), state_(state) {}
      
   protected:
      const Flag *flag_;
      const bool state_;
   };

   /**
    * Flag modification modes.
    */
   enum class FlagMod
      {NONE,   /*!< flag not modified */
       DEF,    /*!< flag modified as defined */
       RES,    /*!< flag reset */
       SET,    /*!< flag set */
       EXCEPT, /*!< flag exceptional */
      };

   extern Flag ZF, CF;
}

#endif

