#include <fstream>
#include <iostream>
#include <string>
#include <map>

#include <getopt.h>

#include "emit.hh"

#define SYSCALL_HANDLER_NAME "syscall_handler"
#define SYSCALL_JUMP_TABLE_NAME "syscall_jump_table"
#define SYSCALL_BAD_NAME "syscall_enosys"

using SyscallMap = std::map<unsigned, std::string>;

void emit_syscall_handler(std::ostream& os, unsigned tablen) {
   os << SYSCALL_HANDLER_NAME << ":" << std::endl;
   emit_inst(os, "xchg", "eax", "dword [rsp]");
   emit_inst(os, "cmp", "eax", tablen);
   emit_inst(os, "jge", SYSCALL_BAD_NAME);
   emit_inst(os, "lea", "rdi", make_string("[rel ", SYSCALL_JUMP_TABLE_NAME, "]"));
   emit_inst(os, "jmp", "[rdi + 8*rax]");
}

void emit_jump_table(std::ostream& os, unsigned tablen, const SyscallMap& map, const char *prefix) {
   os << SYSCALL_JUMP_TABLE_NAME << ":" << std::endl;
   for (unsigned i = 0; i < tablen; ++i) {
      const auto it = map.find(i);
      if (it != map.end()) {
         emit_inst(os, "dq", make_string(prefix, it->second));
      } else {
         emit_inst(os, "dq", SYSCALL_BAD_NAME);
      }
   }
}


int main(int argc, char *argv[]) {
   const auto usage =
      [&] (FILE *f)
      {
         const char *usagestr =
            "usage: %s [-h] [-o <outfile>] [-p <prefix>='syscall_'] <infile>\n" \
            "";
         fprintf(f, usagestr, argv[0]);
      };
   
   const char *optstring = "ho:p:";
   const struct option longopts[] =
      {{"help", no_argument, nullptr, 'h'},
       {"output", required_argument, nullptr, 'o'},
       {"prefix", required_argument, nullptr, 'p'},
       {0}
      };
   int optchar;
   const char *syscallpath = nullptr;
   const char *outpath = nullptr;
   const char *prefix = "syscall_";
   while ((optchar = getopt_long(argc, argv, optstring, longopts, nullptr)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout);
         return 0;
      case 'o':
         outpath = optarg;
         break;
      case 'p':
         prefix = optarg;
         break;
      case '?':
         usage(stderr);
         return 1;
      }
   }

   if ((syscallpath = argv[optind++]) == nullptr) {
      fprintf(stderr, "%s: missing positional argument '<infile>'\n", argv[0]);
      return 1;
   }

   /* read symbols */
   /* <infile> is a table with each column (line) in the following format:
    * SYSCALL_NUMBER SYSCALL_NAME
    */
   std::ifstream syscallf;
   syscallf.open(syscallpath);
   SyscallMap syscallmap;
   unsigned syscallno;
   std::string syscallsym;
   while (syscallf >> syscallno && syscallf >> syscallsym) {
      if (syscallmap.find(syscallno) != syscallmap.end()) {
         fprintf(stderr, "repeated syscall number %u, aborting...\n", syscallno);
         return 2;
      }
      syscallmap.insert({syscallno, syscallsym});
   }

   std::ofstream of;
   std::ostream *os;
   if (outpath) {
      of.open(outpath);
      os = &of;
   } else {
      os = &std::cout;
   }
   
   const unsigned tablen = syscallmap.empty() ? 0 : syscallmap.rbegin()->first + 1;
   emit_syscall_handler(*os, tablen);
   emit_jump_table(*os, tablen, syscallmap, prefix);
   
   return 0;
}
