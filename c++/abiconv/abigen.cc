#include <iostream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <sstream>
#include <forward_list>

#include "util.hh"

enum class type_token {C, S, I, L, LL, P};

static type_token str_to_token(const std::string& s) {
   const std::unordered_map<std::string, type_token> map =
      {{"c", type_token::C},
       {"s", type_token::S},
       {"i", type_token::I},
       {"l", type_token::L},
       {"ll", type_token::LL},
       {"p", type_token::P},
      };

   std::string s_ = strmap(s, tolower);
   auto it = map.find(s_);
   if (it == map.end()) {
      throw std::invalid_argument(std::string("bad token `") + s + "'");
   }
   return it->second;
}

struct signature {
   std::string sym;
   std::forward_list<type_token> params;
};

std::istream& operator>>(std::istream& is, signature& sign) {
   std::string line_tmp;
   std::getline(is, line_tmp);
   std::stringstream line(line_tmp);
   
   if (!(line >> sign.sym)) {
      throw std::invalid_argument("failed to read symbol");
   }

   std::string tokstr;
   while (line >> tokstr) {
      sign.params.push_front(str_to_token(tokstr));
   }

   return is;
}

int main(int argc, char *argv[]) {
   const char *usage = "usage: %s [-h]\n";

   int optchar;
   while ((optchar = getopt(argc, argv, "h")) >= 0) {
      switch (optchar) {
      case 'h':
         printf(usage, argv[0]);
         return 0;
         
      default:
         fprintf(stderr, usage, argv[0]);
         return 1;
      }
   }

   signature sign;
   std::cin >> sign;
   
   // TODO
   
   return 0;
}
