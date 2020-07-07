#include <unordered_map>
#include <sstream>
#include <mach-o/loader.h>

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

template <typename Flag>
void parse_flags(char *optarg, const FlagSet<Flag>& flagset, Flags<Flag>& flags) {
   const char *token;
   bool mask;
   while ((token = strsep(&optarg, ",")) != nullptr) {
      /* get mask */
      switch (*token) {
      case '\0':
         throw std::string("empty flag");
      case '-':
         ++token;
         mask = false;
         break;
      case '+':
         ++token;
      default:
         mask = true;
      }

      /* parse flag */
      std::string flagstr(token);
      auto flagset_it = flagset.find(flagstr);
      if (flagset_it == flagset.end()) {
         std::stringstream ss;
         ss << "invalid flag `" << token << "'";
         throw std::string(ss.str());
      }
      flags.insert({flagset_it->second, mask});
   }
}

template void parse_flags<uint32_t>(char *, const FlagSet<uint32_t>&, Flags<uint32_t>&);

const std::unordered_map<std::string, uint32_t> mach_header_filetype_map = 
   {MH_FLAG(MH_OBJECT),
    MH_FLAG(MH_EXECUTE),
    MH_FLAG(MH_FVMLIB),
    MH_FLAG(MH_CORE),
    MH_FLAG(MH_PRELOAD),
    MH_FLAG(MH_DYLIB),
    MH_FLAG(MH_DYLINKER),
    MH_FLAG(MH_BUNDLE),
    MH_FLAG(MH_DYLIB_STUB),
    MH_FLAG(MH_DSYM),
    MH_FLAG(MH_KEXT_BUNDLE),
   };
