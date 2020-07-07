#include "operation.hh"

int Operation::parsesubopts(char *option) {
   while (*option) {
      char *value;
      int index = getsubopt(&option, keylist().data(), &value);
      if (index < 0) {
         if (suboptarg) {
            throw std::string("invalid suboption `") + suboptarg + "'";
         } else {
            throw std::string("missing suboption");
         }
      }

      try {
         int stat = subopthandler(index);
         if (stat <= 0) {
            return stat;
         }
      } catch (const std::string& s) {
         throw std::string(suboptarg) + ": " + s;
      }
   }

   return 0;
}
