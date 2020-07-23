#ifndef __ASM_HPP
#error "include \"asm.hpp\""
#endif

#ifndef __ASM_CRT_HPP
#define __ASM_CRT_HPP

#include <unordered_map>

namespace zc::z80 {

   class CRT {
      typedef std::pair<Label *, LabelValue *> Pair;
      typedef std::unordered_map<std::string, Pair> Map;
      
   public:
      const Label *label(const std::string& name);
      const LabelValue *val(const std::string& name);

      template <typename... Args>
      CRT(Args... args): map_(args...) {}

      CRT(const std::initializer_list<std::string>& l);
      
   protected:
      Map map_;

      const Pair& add(const std::string& name);
   };

   extern CRT g_crt;
}

#endif
