#include <initializer_list>

#include "asm.hpp"

namespace zc::z80 {

   CRT::CRT(const std::initializer_list<std::string>& l) {
      for (const std::string& str : l) {
         Label *label = new Label(str);
         LabelValue *val = new LabelValue(label);
         map_[str] = std::pair(label, val);
      }
   }

   const CRT::Pair& CRT::add(const std::string& name) {
      Label *label = new Label(name);
      LabelValue *val = new LabelValue(label);
      return map_[name] = Pair(label, val);
   }
   
   const Label *CRT::label(const std::string& name) {
      auto it = map_.find(name);
      if (it == map_.end()) {
         return add(name).first;
      } else {
         return it->second.first;
      }
   }

   const LabelValue *CRT::val(const std::string& name) {
      auto it = map_.find(name);
      if (it == map_.end()) {
         return add(name).second;
      } else {
         return it->second.second;
      }
   }
   
   CRT g_crt {"__longjmp",
              "__memchr",
              "__memcmp",
              "__memcpy",
              "__memmove",
              "__memset",
              "__memclear",
              "_printf",
              "__setjmp",
              "_sprintf",
              "__strcat",
              "__strchr",
              "__strcmp",
              "__strcpy",
              "__strcspn",
              "__strlen",
              "__strncat",
              "__strncmp",
              "__strncpy",
              "__strpbrk",
              "__strrchr",
              "__strspn",
              "__strstr",
              "_strtok",
              "_ret",
              "__bldiy",
              "__bshl",
              "__bshru",
              "__bstiy",
              "__bstix",
              "__case",
              "__case16",
              "__case16D",
              "__case24",
              "__case24D",
              "__case8",
              "__case8D",
              "__frameset",
              "__frameset0",
              "__iand",
              "__icmpzero",
              "__idivs",
              "__idivu",
              "__idvrmu",
              "__ildix",
              "__ildiy",
              "__imul_b",
              "__imulu",
              "__imuls",
              "__indcall",
              "__ineg",
              "__inot",
              "__ior",
              "__irems",
              "__iremu",
              "__ishl",
              "__ishl_b",
              "__ishrs",
              "__ishrs_b",
              "__ishru",
              "__ishru_b",
              "__istix",
              "__istiy",
              "__itol",
              "__ixor",
              "__ladd",
              "__ladd_b",
              "__land",
              "__lcmps",
              "__lcmpu",
              "__lcmpzero",
              "__ldivs",
              "__ldivu",
              "__ldvrmu",
              "__lldix",
              "__lldiy",
              "__lmuls",
              "__lmulu",
              "__lneg",
              "__lnot",
              "__lor",
              "__lrems",
              "__lremu",
              "__lshl",
              "__lshrs",
              "__lshru",
              "__lstix",
              "__lstiy",
              "__lsub",
              "__lxor",
              "__sand",
              "__scmpzero",
              "__sdivs",
              "__sdivu",
              "__seqcase",
              "__seqcaseD",
              "__setflag",
              "__sldix",
              "__sldiy",
              "__smuls",
              "__smulu",
              "__sneg",
              "__snot",
              "__sor",
              "__srems",
              "__sremu",
              "__sshl",
              "__sshl_b",
              "__sshrs",
              "__sshrs_b",
              "__sshru",
              "__sshru_b",
              "__sstix",
              "__sstiy",
              "__stoi",
              "__stoiu",
              "__sxor",
              "__fppack",
              "__fadd",
              "__fcmp",
              "__fdiv",
              "__ftol",
              "__ultof",
              "__ltof",
              "__fmul",
              "__fneg",
              "__fsub",
              "_FLTMAX",
              "_sqrtf",
              "__frbtof",
              "__frftob",
              "__frftoub",
              "__frftoi",
              "__frftoui",
              "__frftos",
              "__frftous",
              "__fritof",
              "__fruitof",
              "__frstof",
              "__frubtof",
              "__frustof",
              "__cdivu",
   };

}
