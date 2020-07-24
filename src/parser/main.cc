#include <iostream>
#include <list>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unistd.h>
#include <clang-c/Index.h>

enum class type_token {C, S, I, L, LL, P,
                       UC, US, UI, UL, ULL,
                       SC,
                       V};



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
static const reg_group r11 = {"r11b", "r11w", "r11d", "r11"};

inline std::ostream& strjoin(std::ostream& os, const std::string& separator) {
   return os;
}

template <typename Head>
std::ostream& strjoin(std::ostream& os, const std::string& separator, Head&& head) {
   return os << head;
}

template <typename Head1, typename Head2, typename... Tail>
std::ostream& strjoin(std::ostream& os, const std::string& separator, Head1&& head1, Head2&& head2,
                      Tail&&... tail) {
   return strjoin(os << head1 << separator, separator, head2, tail...);
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

void emit_load_arg(std::ostream& os, type_token param, const reg_group& group,
                   std::size_t frame_offset) {
   const reg_width param_width_32 = get_token_width_32(param);
   reg_width param_width_64 = get_token_width_64(param);
   const bool sign = get_token_signed(param);
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
   emit_inst(os, opcode, group.reg(param_width_64), src_str);
}


std::ostream& operator<<(std::ostream& os, const CXString& str) {
   os << clang_getCString(str);
   clang_disposeString(str);
   return os;
}

template <typename Func>
void for_each(CXCursor root, Func func) {
   clang_visitChildren(root,
                       [] (CXCursor c, CXCursor parent, CXClientData client_data) {
                          Func func = * (Func *) client_data;
                          return func(c);
                       },
                       &func);
}

template <typename Func>
void for_each(const CXTranslationUnit& unit, Func func) {
   for_each(clang_getTranslationUnitCursor(unit), func);
}



struct ABIConversion {
   using Cursors = std::list<CXCursor>;
   
   std::string sym;
   CXCursor function_decl;
   CXType function_type;
   
   ABIConversion(CXCursor function_decl):
      function_decl(function_decl), function_type(clang_getCursorType(function_decl)) {
      auto cxsym = clang_getCursorSpelling(function_decl);
      sym = clang_getCString(cxsym);
      clang_disposeString(cxsym);
      assert(function_type.kind == CXType_FunctionProto);
   }

   void emit(std::ostream& os, const std::string& override_prefix = "__",
             const std::string& universal_prefix = "_") const {
      os << "\tglobal\t" << universal_prefix << override_prefix << sym << std::endl;
      os << "\textern\t" << universal_prefix << sym << std::endl;

      os << universal_prefix << override_prefix << sym << ":" << std::endl;

      /* check if freshly bound */
      emit_inst(os, "cmp", "qword [rel __dyld_stub_binder_flag]", "0");
      emit_inst(os, "je", ".l1");

      /* fix stack */
      emit_inst(os, "mov", "rsp", "qword [rel __dyld_stub_binder_flag]");
      emit_inst(os, "add", "rsp", "16");
      emit_inst(os, "mov", "qword [rel __dyld_stub_binder_flag]", "0");

      os << ".l1:" << std::endl;
      
      /* save registers */
      emit_inst(os, "push", "rbp");
      emit_inst(os, "mov", "rbp", "rsp");
      emit_inst(os, "push", "rdi");
      emit_inst(os, "push", "rsi");

      /* align stack */
      emit_inst(os, "and", "rsp", "~0xf");

      /* parameter info */
      for (int i = 0; i < clang_getNumArgTypes(function_type); ++i) {
         CXType type = clang_getArgType(function_type, i);
         os << "\t" << clang_getTypeSpelling(type) << std::endl;
      }
   }
   
};

struct ABIGenerator {
   using Syms = std::unordered_set<std::string>;
   
   CXIndex index = clang_createIndex(0, 0);
   Syms syms;
   std::ostream& os;

   ABIGenerator(std::istream& syms_is, std::ostream& os): os(os) {
      std::string tmp_sym;
      while (syms_is >> tmp_sym) {
         syms.insert(tmp_sym);
      }
   }

   ~ABIGenerator() {
      clang_disposeIndex(index);
   }

   void emit_header() {
      os << "\tsegment .text" << std::endl;
      os << "\textern __dyld_stub_binder_flag" << std::endl;
   }

   void handle_file(const std::string& path) {
      CXTranslationUnit unit = clang_parseTranslationUnit(index, path.c_str(), nullptr, 0, nullptr,
                                                          0, CXTranslationUnit_None);
      if (unit == nullptr) {
         std::cerr << "Unable to parse translation unit. Quitting." << std::endl;
         exit(-1);
      }
      
      for_each(unit, [&] (CXCursor c) { return handle_cursor(c); });
      
      clang_disposeTranslationUnit(unit);
   }

   CXChildVisitResult handle_cursor(CXCursor c) {
      switch (clang_getCursorKind(c)) {
      case CXCursor_FunctionDecl:
         handle_function_decl(c);
         break;
      default:
         break;
      }
      return CXChildVisit_Recurse;
   }

   void handle_function_decl(CXCursor c) {
      ABIConversion conv(c);
      auto syms_it = syms.find(conv.sym);
      if (syms_it != syms.end()) {
         syms.erase(syms_it);
         conv.emit(os);
      }
   }
   
};

int main(int argc, char *argv[]) {
   auto usage = [=] (FILE *f) {
                   const char *usage = "usage: %s [-h] [-s <symfile>] [-o <outpath>] <header>...\n";
                   fprintf(f, usage, argv[0]);
                };

   const char *sympath = nullptr;
   const char *outpath = nullptr;
   
   const char *optstring = "hs:o:";
   int optchar;
   if ((optchar = getopt(argc, argv, optstring)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout);
         return 0;
      case 's':
         sympath = optarg;
         break;
      case 'o':
         outpath = optarg;
         break;
      case '?':
         usage(stderr);
         return 1;
      }
   }

   /* open symbol stream */
   std::ifstream symf;
   if (sympath) {
      symf.open(sympath);
   }
   std::istream& sym_is = sympath ? symf : std::cin;

   std::ofstream of;
   if (outpath) {
      of.open(outpath);
   }
   std::ostream& os = outpath ? of : std::cout;

   ABIGenerator abigen(sym_is, os);
   abigen.emit_header();
   
   /* handle each header */
   for (int i = optind; i < argc; ++i) {
      abigen.handle_file(argv[i]);
   }

   return 0;
}
