#include "util.hh"

std::string strmap(const std::string& s, int (*map)(int)) {
   std::string result;

   for (const char c : s) {
      result.push_back(map(c));
   }

   return result;
}

std::string strfilter(const std::string& s, int (*filter)(int)) {
   std::string result;

   for (const char c : s) {
      if (filter(c)) {
         result.push_back(c);
      }
   }

   return result;
}
