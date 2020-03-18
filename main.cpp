#include <cstdio>
#include <unistd.h>
#include <iostream>

#include <LIEF/LIEF.hpp>
extern "C" {
#include <xed/xed-interface.h>
}

#include "instrs.hpp"
#include "function.hpp"

void usage(FILE *f, int argc, char *argv[]) {
   fprintf(f,
           "usage: %1$s <macho>\n"              \
           "       %1$s -h\n",
           argv[0]
           );
}

int main(int argc, char *argv[]) {
   const char *optstring = "h";
   int optchar;
   while ((optchar = getopt(argc, argv, optstring)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout, argc, argv);
         return 0;
      case '?':
         usage(stderr, argc, argv);
         return 1;
      }
   }

   if (argc - optind != 1) {
      usage(stderr, argc, argv);
      return 1;
   }

   const char *in_path = argv[optind++];

   // XED initialization
   xed_tables_init();
 
   std::unique_ptr<LIEF::MachO::FatBinary> macho = LIEF::MachO::Parser::parse(in_path);

   for (auto&& binary : *macho) {
      for (auto& func : binary.functions()) {
         std::cout << func.name() << func << std::endl;
      }
   }
   
   return 0;
}

