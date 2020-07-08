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

      return 0;

   while (t.size() > 0) {
      for (auto it = t.begin(); it != t.end(); ++it) {
         std::cout << *it << std::endl;
      }
      
      std::cout << std::endl;

      auto it = t.begin();
      for (int i = random() % t.size(); i > 0; --i) {
         ++it;
      }
      t.erase(it);
   }

   return 0;
}
