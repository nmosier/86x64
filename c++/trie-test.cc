#include <string>
#include <iostream>
#include <iterator>

#include "trie.hh"

int main(int argc, char *argv[]) {
   trie<char, std::string> t;

   if (argc > 1) {
      for (int i = 1; i < argc; ++i) {
         std::string arg(argv[i]);
         t.insert(arg.begin(), arg.end());
      }
   } else {
      std::string s;
      while (std::cin >> s) {
         t.insert(s);
      }
   }

   for (auto it = t.begin(); it != t.end(); ++it) {
      std::cout << *it << std::endl;
   }
      
   std::cout << std::endl;

   for (auto it = t.begin(); it != t.end(); it = t.erase(it)) {}
   
   for (auto it = t.begin(); it != t.end(); ++it) {
      std::cout << *it << std::endl;
   }

   return 0;
}
