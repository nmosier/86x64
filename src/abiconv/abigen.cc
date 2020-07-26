#include <iostream>
#include <list>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unistd.h>
#include <clang-c/Index.h>

template <typename CXT, typename Func>
std::string to_string_aux(CXT val, Func func) {
   CXString cxs = func(val);
   std::string str = clang_getCString(cxs);
   clang_disposeString(cxs);
   return str;
}

std::string to_string(CXType type) { return to_string_aux(type, clang_getTypeSpelling); }
std::string to_string(CXCursorKind kind) {
   return to_string_aux(kind, clang_getCursorKindSpelling);
}
std::string to_string(CXCursor cursor) { return to_string_aux(cursor, clang_getCursorSpelling); }
std::string to_string(CXTypeKind kind) { return to_string_aux(kind, clang_getTypeKindSpelling) ;}

template <typename T>
constexpr T div_up(T a, T b) {
   return (a + b - 1) / b;
}

template <typename T>
constexpr T align_up(T n, T align) {
   return div_up(n, align) * align;
}

enum class arch {i386, x86_64};

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

static bool get_type_signed(CXType type) {
   switch (type.kind) {
   case CXType_Invalid:
   case CXType_Unexposed:
   case CXType_Void:
      throw std::invalid_argument("invalid type");
      
   case CXType_Pointer:
   case CXType_IncompleteArray:
   case CXType_ConstantArray:
   case CXType_Bool:
   case CXType_UChar:
   case CXType_Char_U:
   case CXType_UShort:
   case CXType_UInt:
   case CXType_ULong:
   case CXType_ULongLong:
      return false;
      
   case CXType_SChar:
   case CXType_Char_S:
   case CXType_Short:
   case CXType_Int:
   case CXType_Long:
   case CXType_LongLong:
   case CXType_Enum:
      return true;

   default:
      throw std::invalid_argument("unhandled type '" + to_string(type) + "' with kind '" +
                                  to_string(type.kind) + "'");
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

reg_width get_type_width(CXType type, arch a) {
   switch (type.kind) {
   case CXType_Invalid:
   case CXType_Unexposed:
   case CXType_Void:
      throw std::invalid_argument("invalid type");
   case CXType_Bool:
   case CXType_UChar:
   case CXType_Char_U:
   case CXType_SChar:
   case CXType_Char_S:
      return reg_width::B;
   case CXType_Short:
   case CXType_UShort:
      return reg_width::W;
   case CXType_Int:
   case CXType_UInt:
   case CXType_Enum:
      return reg_width::D;
   case CXType_Long:
   case CXType_ULong:
      return a == arch::i386 ? reg_width::D : reg_width::Q;
   case CXType_LongLong:
   case CXType_ULongLong:
      return reg_width::Q;

   case CXType_Pointer:
   case CXType_IncompleteArray:
   case CXType_ConstantArray:
      return a == arch::i386 ? reg_width::D : reg_width::Q;

   default:
      throw std::invalid_argument("unhandled type '" + to_string(type) + "' with kind '" +
                                  to_string(type.kind) + "'");
   }
}

void emit_load_arg(std::ostream& os, CXType param, const reg_group& group,
                   std::size_t frame_offset) {
   const reg_width param_width_32 = get_type_width(param, arch::i386);
   reg_width param_width_64 = get_type_width(param, arch::x86_64);
   const bool sign = get_type_signed(param);
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
   src << "\t" << "; " << clang_getTypeSpelling(param);
   std::string src_str = src.str();
   emit_inst(os, opcode, group.reg(param_width_64), src_str);
}

struct ABIConversion {
   using Cursors = std::list<CXCursor>;
   
   std::string sym;
   // CXCursor function_decl;
   CXType function_type;

   ABIConversion(CXCursor function_decl): function_type(clang_getCursorType(function_decl))
   {
      const std::string symprefix = "_";
      auto cxsym = clang_getCursorSpelling(function_decl);
      sym = symprefix + clang_getCString(cxsym);
      clang_disposeString(cxsym);
      assert(function_type.kind == CXType_FunctionProto);
   }

   ABIConversion(CXType function_type, const std::string& sym):
      sym(sym), function_type(function_type) {}

   static CXType handle_type(CXType type) {
      return clang_getCanonicalType(type);
   }

   void emit(std::ostream& os, const std::string& override_prefix = "__") const {
      const bool variadic = clang_isFunctionTypeVariadic(function_type);
      
      os << "\tglobal\t" << override_prefix << sym << std::endl;
      os << "\textern\t" << sym << std::endl;

      os << override_prefix << sym << ":" << std::endl;

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

      /* transfer arguments */
      const std::list<const reg_group *> regs = {&rdi, &rsi, &rdx, &rcx, &r8, &r9};
      std::size_t frame_offset = 12;
      int param_it;
      int param_end = clang_getNumArgTypes(function_type);
      auto reg_it = regs.begin();
      for (param_it = 0; param_it != param_end && reg_it != regs.end(); ++param_it, ++reg_it) {
         CXType param_type = handle_type(clang_getArgType(function_type, param_it));

         emit_load_arg(os, param_type, **reg_it, frame_offset);
         frame_offset += align_up<size_t>(reg_width_size(get_type_width(param_type, arch::i386)),
                                          4);
      }

      if (param_it != param_end && reg_it == regs.end()) {
         fprintf(stderr, "registers exhaused\n");
         abort();
      }

      /* variadic function placeholder */
      if (variadic) {
         emit_inst(os, "xor", "eax", "eax");
      }

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
   
};

struct ABIGenerator {
   using Syms = std::unordered_set<std::string>;
   
   CXIndex index = clang_createIndex(0, 0);
   std::ostream& os;

   ABIGenerator(std::ostream& os): os(os) {}

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
      case CXCursor_AsmLabelAttr:
         handle_asm_label_attr(c);
         break;
      default:
         break;
      }
      return CXChildVisit_Recurse;
   }

   void handle_function_decl(CXCursor c) {
      ABIConversion conv(c);
      conv.emit(os);
   }

   void handle_asm_label_attr(CXCursor c) {
      ABIConversion conv(clang_getCursorType(clang_getCursorLexicalParent(c)), to_string(c));
      conv.emit(os);
   }
   
};

int main(int argc, char *argv[]) {
   auto usage = [=] (FILE *f) {
                   const char *usage = "usage: %s [-h] [-o <outpath>] <header>...\n";
                   fprintf(f, usage, argv[0]);
                };

   const char *outpath = nullptr;
   
   const char *optstring = "ho:";
   int optchar;
   if ((optchar = getopt(argc, argv, optstring)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout);
         return 0;
      case 'o':
         outpath = optarg;
         break;
      case '?':
         usage(stderr);
         return 1;
      }
   }

   std::ofstream of;
   if (outpath) {
      of.open(outpath);
   }
   std::ostream& os = outpath ? of : std::cout;

   ABIGenerator abigen(os);
   abigen.emit_header();
   
   /* handle each header */
   for (int i = optind; i < argc; ++i) {
      abigen.handle_file(argv[i]);
   }

   return 0;
}
