#include <string>
#include <iostream>
#include <iterator>

#include "trie.hh"

int main(int argc, char *argv[]) {
   trie_map<char, std::string, int> t;

   if (argc > 1) {
      for (int i = 1; i < argc; ++i) {
         std::string arg(argv[i]);
         t.insert(arg, i - 1);
      }
   } else {
      int i = 0;
      std::string s;
      while (std::cin >> s) {
         t.insert(s, i++);
      }
   }

   for (auto it = t.begin(); it != t.end(); ++it) {
      std::cout << it->first << " " << it->second << std::endl;
   }
      
   std::cout << std::endl;

#if 1
   for (auto it = t.begin(); it != t.end(); it = t.erase(it)) {}
   
   for (auto it = t.begin(); it != t.end(); ++it) {
      std::cout << it->first << " " << it->second << std::endl;
   }
#endif

   return 0;
}
