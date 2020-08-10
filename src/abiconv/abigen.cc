#include <iostream>
#include <list>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <clang-c/Index.h>

#include "emit.hh"
#include "util.hh"
#include "abigen.hh"
#include "typeinfo.hh"
#include "typeconv.hh"

bool force_all = false;

template <typename RegIt>
struct param_info {
   RegIt reg_it;
   const RegIt reg_end;
   unsigned fp_idx = 0;
   const unsigned fp_end;

   param_info(RegIt reg_begin, RegIt reg_end, unsigned fp_count):
      reg_it(reg_begin), reg_end(reg_end), fp_idx(0), fp_end(fp_count) {}
};

struct ABIConversion {
   using Cursors = std::list<CXCursor>;
   
   std::string sym;
   CXType function_type;

   static constexpr unsigned max_reg_args = 6;
   static constexpr unsigned max_xmm_args = 8;

   using Regs = std::list<const reg_group *>;
   Regs regs;

   virtual void emit_call(std::ostream& os) const = 0;

   ABIConversion(CXCursor function_decl, const Regs& regs):
      function_type(clang_getCursorType(function_decl)), regs(regs) {
      auto cxsym = clang_getCursorSpelling(function_decl);
      sym = std::string("_") + clang_getCString(cxsym);
      clang_disposeString(cxsym);
      assert(function_type.kind == CXType_FunctionProto);
   }

   ABIConversion(CXType function_type, const std::string& sym, const Regs& regs):
      sym(sym), function_type(function_type), regs(regs) {}

   virtual ~ABIConversion() {}
   
   static CXType handle_type(CXType type) {
      return clang_getCanonicalType(type);
   }

   unsigned argc() const {
      const int tmp = clang_getNumArgTypes(function_type);
      assert(tmp >= 0);
      return tmp;
   }

   static CXTypeKind get_arg_kind(CXTypeKind kind) {
      switch (kind) {
      case CXType_ConstantArray: return CXType_Pointer;
      default: return kind;
      }
   }

   /* assumes stack is 16-byte aligned */
   size_t stack_args_size() const {
      size_t size = 0;
      unsigned reg_i = 0;
      unsigned xmm_i = 0;
      
      for (unsigned argi = 0; argi < argc(); ++argi) {
         const CXType argtype = clang_getCanonicalType(clang_getArgType(function_type, argi));
         switch (get_type_domain(get_arg_kind(argtype.kind))) {
         case type_domain::INT:
            if (reg_i < max_reg_args) {
               ++reg_i;
            } else {
               size += std::max<size_t>(sizeof_type(argtype, arch::x86_64), 8);
            }
            break;
         case type_domain::REAL:
            if (xmm_i < max_xmm_args) {
               ++xmm_i;
            } else {
               size += std::max<size_t>(sizeof_type(argtype, arch::x86_64), 8);
            }
            break;
         default: abort();
         }
         
      }
      
      return align_up<size_t>(size, 16);
   }

   size_t stack_data_size() const {
      size_t size = 0;

      for (unsigned argi = 0; argi < argc(); ++argi) {
         CXType argtype = clang_getCanonicalType(clang_getArgType(function_type, argi));

         std::optional<CXType> size_type;
         switch (argtype.kind) {
         case CXType_Pointer:
            size_type = clang_getPointeeType(argtype);
            break;
         case CXType_ConstantArray:
            size_type = argtype;
            break;
         default:
            break;
         }

         if (size_type) {
            align_up(size, alignof_type(*size_type, arch::x86_64));
            size += sizeof_type(*size_type, arch::x86_64);
         }
      }

      return align_up<size_t>(size, 16);
   }


#if 0
   void emit_function_call(std::ostream& os, Symbols& symbols, const Symbols& ignore_structs) {
      const std::list<const reg_group *> regs {&rdi, &rsi, &rdx, &rcx, &r8, &r9};
      emit(os, symbols, ignore_structs, regs.begin(), regs.end(),
           [] (std::ostream& os, const std::string& sym) {
              emit_inst(os, "call", sym);
           });      
   }

   void emit_system_call(std::ostream& os, Symbols& symbols, const Symbols& ignore_structs) {
      /* NOTE: %rax isn't really a parameter, though treating it as such preserves it during
       * transformation process.
       */
      const std::list<const reg_group *> regs {&rax, &rdi, &rsi, &rdx, &r10, &r8, &r9};
      emit(os, symbols, ignore_structs, regs.begin(), regs.end(),
           [] (std::ostream& os, const std::string& sym) {
              emit_inst(os, "syscall");
           });
   }
#endif

   void emit(std::ostream& os, Symbols& symbols, const Symbols& ignore_structs) {
      const bool variadic = clang_isFunctionTypeVariadic(function_type);
      const std::string& override_prefix = "__";

      if (variadic) {
         /* skip variadic functions */
         return;
      }
      
      if (symbols.find(sym) == symbols.end()) {
         return;
      }
      symbols.erase(sym);
      

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

      /* make space on stack */
      MemoryLocation stack_args(rsp, 0);
      // MemoryLocation stack_data(rsp, stack_args_size());
      unsigned label = 0;
      conversion to_conv(true, arch::i386, arch::x86_64, {rsp, static_cast<int>(stack_args_size())},
                         label, ignore_structs);
      conversion from_conv(false, arch::x86_64, arch::i386,
                           {rsp, static_cast<int>(stack_args_size())}, label, ignore_structs);
      
      emit_inst(os, "sub", "rsp", stack_data_size() + stack_args_size());
      
      /* transfer arguments */
      param_info info(regs.begin(), regs.end(), 8);
      int param_it;
      int param_end = clang_getNumArgTypes(function_type);
      MemoryLocation load_loc(rbp, 12);

      std::stringstream to_ss;
      std::stringstream from_ss;

      for (param_it = 0;
           param_it != param_end;
           ++param_it) {

         CXType type = handle_type(clang_getArgType(function_type, param_it));

         std::unique_ptr<Location> src;
         std::unique_ptr<Location> dst;
         switch (get_type_domain(get_arg_kind(type.kind))) {
         case type_domain::INT:
            if (info.reg_it != info.reg_end) {
               dst = std::make_unique<RegisterLocation>(**info.reg_it++);
            } else {
               dst = std::make_unique<MemoryLocation>(stack_args);
            }
            break;
            
         case type_domain::REAL:
            if (info.fp_idx != info.fp_end) {
               dst = std::make_unique<SSELocation>(info.fp_idx++);
            } else {
               dst = std::make_unique<MemoryLocation>(stack_args);
            }
         }

         CXTypeKind type_kind;
         switch (type.kind) {
         case CXType_ConstantArray:
            type_kind = CXType_Pointer;
            to_conv.convert_pointer(to_ss, type, load_loc, *dst);
            from_conv.convert_pointer(from_ss, type, *dst, load_loc);
            break;

         default:
            type_kind = type.kind;
            to_conv.convert(to_ss, type, load_loc, *dst);
            from_conv.convert(from_ss, type, *dst, load_loc);
            break;
         }

         load_loc += align_up<size_t>(sizeof_type(type_kind, arch::i386), 4);
         if (dst->kind() == Location::Kind::MEM) {
            stack_args += align_up<size_t>(sizeof_type(type, arch::x86_64), 8);
         }

      }

      /* convert from i386 to x86_64 */
      os << to_ss.str();
      
      /* call */
      emit_call(os);

      /* convert from x86_64 to i386 */
      os << from_ss.str();

      // emit_inst(os, "add", "rsp", stack_data_size() + stack_args_size());
      emit_inst(os, "lea", "rsp", "[rbp - 0x10]");
      
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

struct FunctionConversion: ABIConversion {
   template <typename... Args>
   FunctionConversion(Args&&... args):
      ABIConversion(args..., {&rdi, &rsi, &rdx, &rcx, &r8, &r9}) {}

   virtual void emit_call(std::ostream& os) const override {
      emit_inst(os, "call", sym);
   }
};

struct SyscallConversion: ABIConversion {
   template <typename... Args>
   SyscallConversion(Args&&... args):
      ABIConversion(args..., {&rax, &rdi, &rsi, &rdx, &r10, &r8, &r9}) {}

   virtual void emit_call(std::ostream& os) const override {
      emit_inst(os, "syscall");
   }
};

struct ABIGenerator {
   CXIndex index = clang_createIndex(0, 0);
   std::ostream& os;
   Symbols symbols;
   Symbols ignore_structs;
   enum class ABI {FUNCTION, SYSCALL} abi;
   bool force_all;

   ABIGenerator(std::ostream& os, ABI abi): os(os), abi(abi) {}

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
      
      for_each(unit, [&] (CXCursor c, CXCursor p) { return handle_cursor(c, p); });
      
      clang_disposeTranslationUnit(unit);
   }

   CXChildVisitResult handle_cursor(CXCursor c, CXCursor p) {
      switch (clang_getCursorKind(c)) {
      case CXCursor_FunctionDecl:
         handle_function_decl(c);
         break;
      case CXCursor_AsmLabelAttr:
         handle_asm_label_attr(c, p);
         break;
      default:
         break;
      }
      return CXChildVisit_Recurse;
   }

   void handle_function_decl(CXCursor c) {
      switch (clang_getCursorType(c).kind) {
      case CXType_BlockPointer:
         return;
      default:
         break;
      }

      std::unique_ptr<ABIConversion> conv(make_conv(c));
      conv->emit(os, symbols, ignore_structs);
   }

   void handle_asm_label_attr(CXCursor c, CXCursor p) {
      const std::string sym = to_string(c);
      if (sym.find('$') == std::string::npos) {
         return; /* this is something else */
      }
      std::unique_ptr<ABIConversion> conv(make_conv(clang_getCursorType(p), sym));
      conv->emit(os, symbols, ignore_structs);
   }

   template <typename... Args>
   ABIConversion *make_conv(Args&&... args) const {
      switch (abi) {
      case ABI::FUNCTION:
         return new FunctionConversion(args...);
      case ABI::SYSCALL:
         return new SyscallConversion(args...);
      default: abort();
      }
   }
};

template <typename Op>
void parse_syms(std::istream& is, Op op) {
   std::string tmp;
   while (is >> tmp) {
      op(tmp);
   }
}

template <typename Op>
void parse_syms(const char *path, Op op) {
   std::ifstream is;
   is.open(path);
   parse_syms(is, op);
}

template <typename Op>
void parse_lines(const char *path, Op op) {
   std::ifstream is;
   is.open(path);
   std::string line;
   while (getline(is, line)) {
      op(line);
   }
}

int main(int argc, char *argv[]) {
   auto usage = [=] (FILE *f) {
                   const char *usage =
                      "usage: %s [option...] header...\n"               \
                      "Options:\n"                                      \
                      "  -h              print help dialog\n"           \
                      "  -o <path>       output asm file path (if omitted, defaults to stdin\n" \
                      "  -s <symfile>    file containing symbols to consider\n" \
                      "  -i <ignorefile> file containing symbols to ignore\n" \
                      "  -r <structfile> file containing struct names to not convert\n" \
                      "  -c              use system call ABI"           \
                      "";
                   fprintf(f, usage, argv[0]);
                };
   
   const char *outpath = nullptr;
   const char *sympath = nullptr;
   const char *symignorepath = nullptr;
   const char *structpath = nullptr;
   ABIGenerator::ABI abi = ABIGenerator::ABI::FUNCTION;
   const char *optstring = "ho:s:i:r:";
   const struct option longopts[] = {{"help", no_argument, nullptr, 'h'},
                                     {"output", required_argument, nullptr, 'o'},
                                     {"symfile", required_argument, nullptr, 's'},
                                     {"ignorefile", required_argument, nullptr, 'i'},
                                     {"structfile", required_argument, nullptr, 'r'},
                                     {"syscall", required_argument, nullptr, 'c'},
                                     {0}
   };
   
   int optchar;
   while ((optchar = getopt_long(argc, argv, optstring, longopts, nullptr)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout);
         return 0;
      case 'o':
         outpath = optarg;
         break;
      case 's':
         sympath = optarg;
         break;
      case 'i':
         symignorepath = optarg;
         break;
      case 'r':
         structpath = optarg;
         break;
      case 'c':
         abi = ABIGenerator::ABI::SYSCALL;
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

   ABIGenerator abigen(os, abi);

   if (sympath) {
      parse_syms(sympath, [&] (const std::string& s) { abigen.symbols.insert(s); });
   } else {
      parse_syms(std::cin, [&] (const std::string& s) { abigen.symbols.insert(s); });
   }
   
   if (symignorepath) {
      parse_syms(symignorepath, [&] (const std::string& s) { abigen.symbols.erase(s); });
   }

   if (structpath) {
      parse_lines(structpath, [&] (const std::string& s) { abigen.ignore_structs.insert(s); });
   }

   abigen.emit_header();
   
   /* handle each header */
   for (int i = optind; i < argc; ++i) {
      abigen.handle_file(argv[i]);
   }

   return 0;
}
