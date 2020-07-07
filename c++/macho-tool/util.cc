#include <unordered_map>
#include <sstream>

#include "util.hh"

bool stobool(const std::string& s_) {
   std::string s = s_;
   for (char& c : s) {
      c = tolower(c);
   }
   
   std::unordered_map<std::string, bool> map =
      {{"t", true},
       {"f", false},
       {"true", true},
       {"false", false},
      };

   const auto it = map.find(s);
   if (it != map.end()) {
      return it->second;
   } else {
      /* try to convert to integer */
      char *end;
      long tmp = std::strtol(s.c_str(), &end, 0);
      if (s == "" || *end != '\0') {
         std::stringstream ss;
         ss << "invalid boolean `" << s << "'";
         throw ss.str();
      }

      switch (tmp) {
      case 0:  return false;
      case 1:  return true;
      default: throw std::string("boolean value must be 0 or 1");
      }
   }
}
