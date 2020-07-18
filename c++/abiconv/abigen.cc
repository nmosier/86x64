#include <iostream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <sstream>
#include <list>

#include "util.hh"

enum class type_token {C, S, I, L, LL, P,
                       UC, US, UI, UL, ULL,
                       SC};

static type_token str_to_token(const std::string& s) {
   const std::unordered_map<std::string, type_token> map =
      {{"c", type_token::C},
       {"s", type_token::S},
       {"i", type_token::I},
       {"l", type_token::L},
       {"ll", type_token::LL},
       {"p", type_token::P},
       {"uc", type_token::UC},
       {"us", type_token::US},
       {"ui", type_token::UI},
       {"ul", type_token::UL},
       {"ull", type_token::ULL},
       {"sc", type_token::SC},
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
   std::list<type_token> params;

   void Emit(std::ostream& os, const std::string& prefix);
};

static std::istream& operator>>(std::istream& is, signature& sign) {
   std::string line_tmp;
   std::getline(is, line_tmp);
   std::stringstream line(line_tmp);
   
   if (!(line >> sign.sym)) {
      throw std::invalid_argument("failed to read symbol");
   }

   std::string tokstr;
   while (line >> tokstr) {
      sign.params.push_back(str_to_token(tokstr));
   }

   return is;
}

template <typename Opcode>
static std::ostream& emit_inst(std::ostream& os, Opcode&& opcode) {
   return os << "\t" << opcode << std::endl;
}

template <typename Opcode, typename OperandHead, typename... OperandTail>
static std::ostream& emit_inst(std::ostream& os, Opcode&& opcode, OperandHead&& head,
                               OperandTail&&... tail) {
   return strjoin(os << "\t" << opcode << "\t", ",\t", head, tail...) << std::endl;
}

enum class reg_width {B, W, D, Q};

static reg_width get_token_width(type_token tok) {
   switch (tok) {
   case type_token::C:
   case type_token::UC:
   case type_token::SC:
      return reg_width::B;
   case type_token::S:
   case type_token::US:
      return reg_width::W;
   case type_token::I:
   case type_token::UI:
   case type_token::L:
   case type_token::UL:
   case type_token::P:
      return reg_width::D;
   case type_token::LL:
   case type_token::ULL:
      return reg_width::Q;
   default: throw std::invalid_argument("bad token");
   }
}

struct reg_group {
   const std::string reg_b;
   const std::string reg_w;
   const std::string reg_d;
   const std::string reg_q;
   
   const std::string& reg(reg_width width) const {
      switch (width) {
      case reg_width::B: return reg_b;
      case reg_width::W: return reg_w;
      case reg_width::D: return reg_d;
      case reg_width::Q: return reg_q;
      default: throw std::invalid_argument("bad register width");
      }
   }
};

static const reg_group rdi = {"dil", "di",  "edi", "rdi"};
static const reg_group rsi = {"sil", "si",  "esi", "rsi"};
static const reg_group rdx = {"dl",  "dx",  "edx", "rdx"};
static const reg_group rcx = {"cl",  "cx",  "ecx", "rcx"};
static const reg_group r8  = {"r8b", "r8w", "r8d", "r8"};
static const reg_group r9  = {"r9b", "r9w", "r9d", "r9"};

void signature::Emit(std::ostream& os, const std::string& prefix) {
   os << prefix << sym << ":" << std::endl;

   /* save registers */
   emit_inst(os, "push", "rbp");
   emit_inst(os, "mov", "rbp", "rsp");
   emit_inst(os, "push", "rdi");
   emit_inst(os, "push", "rsi");

   /* align stack */
   emit_inst(os, "and", "rsp", "~0xf");

   /* transfer arguments */
   const std::list<const reg_group *> arg_regs =
      {&rdi, &rsi, &rdx, &rcx, &r8, &r9};

   // NOTE: Don't support ll's yet, so each arg has 4-byte alignment.
   /*  ...
    *  arg2
    *  arg1
    *  ret
    *  caller rbp <-- callee rbp
    */
   std::size_t frame_offset;
   for (auto param_it = params.rbegin(); param_it != params.rend(); ++param_it) {
      
   }
   

   
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
   sign.Emit(std::cout, "__");
   
   // TODO
   
   return 0;
}
