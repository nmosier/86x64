#include <iostream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <sstream>
#include <list>

#include "util.hh"

enum class type_token {C, S, I, L, LL, P,
                       UC, US, UI, UL, ULL,
                       SC,
                       V};

static type_token str_to_token(const std::string& s) {
   const std::unordered_map<std::string, type_token> map =
      {
       {"c",   type_token::C},
       {"s",   type_token::S},
       {"i",   type_token::I},
       {"l",   type_token::L},
       {"ll",  type_token::LL},
       {"p",   type_token::P},
       {"uc",  type_token::UC},
       {"us",  type_token::US},
       {"ui",  type_token::UI},
       {"ul",  type_token::UL},
       {"ull", type_token::ULL},
       {"sc",  type_token::SC},
       {"v",   type_token::V},
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
   std::optional<unsigned> varargs; /*!< unset if no varargs */

   void Emit(std::ostream& os, const std::string& prefix);
};

static std::istream& operator>>(std::istream& is, signature& sign) {
   std::string line_tmp;
   std::getline(is, line_tmp);
   std::stringstream line(line_tmp);
   
   line >> sign.sym;

   std::string tokstr;
   while (line >> tokstr) {
      auto tok = str_to_token(tokstr);
      switch (tok) {
      case type_token::V:
         /* get vararg count */
         sign.varargs = 0;
         line >> *sign.varargs;
         break;
      default:
         sign.params.push_back(tok);
         break;
      }
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

size_t reg_width_size(reg_width width) {
   switch (width) {
   case reg_width::B: return 1;
   case reg_width::W: return 2;
   case reg_width::D: return 4;
   case reg_width::Q: return 8;
   default: throw std::invalid_argument("bad register width");
   }
}

static const char *reg_width_to_str(reg_width width) {
   switch (width) {
   case reg_width::B: return "byte";
   case reg_width::W: return "word";
   case reg_width::D: return "dword";
   case reg_width::Q: return "qword";
   default: throw std::invalid_argument("bad register width");
   }
}

static reg_width get_token_width_32(type_token tok) {
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

static reg_width get_token_width_64(type_token tok) {
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
      return reg_width::D;
   case type_token::L:
   case type_token::UL:
   case type_token::P:
   case type_token::LL:
   case type_token::ULL:
      return reg_width::Q;
   default: throw std::invalid_argument("bad token");
   }
}

static bool get_token_signed(type_token tok) {
   switch (tok) {
   case type_token::C:
      return std::is_signed<char>();
   case type_token::UC:
   case type_token::US:
   case type_token::UI:
   case type_token::UL:
   case type_token::ULL:
   case type_token::P:
      return false; // unsigned
   case type_token::SC:
   case type_token::S:
   case type_token::I:
   case type_token::L:
   case type_token::LL:
      return true; // signed
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
   os << "\tglobal\t" << prefix << sym << std::endl;
   os << "\textern\t" << sym << std::endl;
   
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
   std::size_t frame_offset = 12;
   auto param_it = params.rbegin();
   auto reg_it = arg_regs.begin(); 
   for (; param_it != params.rend() && reg_it != arg_regs.end(); ++param_it, ++reg_it) {
      const type_token param_tok = *param_it;
      const reg_width param_width_32 = get_token_width_32(param_tok);
      reg_width param_width_64 = get_token_width_64(param_tok);
      const bool sign = get_token_signed(param_tok);

      
      const char *opcode;

      
      if (param_width_32 == param_width_64) {
         opcode = "mov";
      } else if (param_width_32 == reg_width::D && param_width_64 == reg_width::Q && !sign) {
         opcode = "mov";
         param_width_64 = param_width_32;
      } else if (sign) {
         opcode = "movsx";
      } else {
         opcode = "movzx";
      }
      
      std::stringstream src;
      src << reg_width_to_str(param_width_32) << " [rbp + " << frame_offset << "]";
      std::string src_str = src.str();
      
      emit_inst(os, opcode, (*reg_it)->reg(param_width_64), src_str);
      
      frame_offset += 4;
   }

   // TODO: Stand-in for variadi functions
   emit_inst(os, "xor", "eax", "eax");

   /* call */
   emit_inst(os, "call", sym);

   /* cleanup */
   emit_inst(os, "pop", "rsi");
   emit_inst(os, "pop", "rdi");
   emit_inst(os, "leave");

   /* return */
   emit_inst(os, "mov", "r11d", "dword [rsp]");
   emit_inst(os, "add", "rsp", "4");
   emit_inst(os, "jmp", "r11");
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

   std::cout << "\tsegment .text" << std::endl;
   
   while (std::cin >> sign) {
      sign.Emit(std::cout, "__");
      std::cout << std::endl;
   }
   
   return 0;
}
