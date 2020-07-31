#include <iostream>
#include <list>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <clang-c/Index.h>

#include "util.hh"
#include "abigen.hh"
#include "typeinfo.hh"
#include "typeconv.hh"

bool force_all = false;

using reg_groups = std::list<const reg_group *>;

struct param_info {
   using reg_iterator = typename reg_groups::const_iterator;
   reg_iterator reg_it;
   const reg_iterator reg_end;
   unsigned fp_idx = 0;
   const unsigned fp_end;

   param_info(const reg_groups& groups, unsigned fp_count):
      reg_it(groups.begin()), reg_end(groups.end()), fp_idx(0), fp_end(fp_count) {}
};

struct ABIConversion {
   using Cursors = std::list<CXCursor>;
   
   std::string sym;
   CXType function_type;

   static constexpr unsigned max_reg_args = 6;
   static constexpr unsigned max_xmm_args = 8;

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

            // std::cerr << to_string(*size_type) << " " << size << std::endl;
         }
      }

      // std::cerr << "=====" << std::endl;

      return align_up<size_t>(size, 16);
   }

   void emit(std::ostream& os, Symbols& symbols, const Symbols& ignore_structs) const {
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
      conversion conv(true, arch::i386, arch::x86_64, {rsp, static_cast<int>(stack_args_size())},
                      ignore_structs);
      
      emit_inst(os, "sub", "rsp", stack_data_size() + stack_args_size());
      
      /* transfer arguments */
      const reg_groups regs = {&rdi, &rsi, &rdx, &rcx, &r8, &r9};
      param_info info(regs, 8);
      int param_it;
      int param_end = clang_getNumArgTypes(function_type);
      MemoryLocation load_loc(rbp, 12);

      for (param_it = 0;
           param_it != param_end /* && reg_it != regs.end() */;
           ++param_it /*, ++reg_it */) {

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
            conv.convert_pointer(os, type, load_loc, *dst);
            conv.convert_void_pointer(os, load_loc, *dst);
            break;

         default:
            type_kind = type.kind;
            conv.convert(os, type, load_loc, *dst);
            break;
         }

         load_loc += align_up<size_t>(sizeof_type(type_kind, arch::i386), 4);
         if (dst->kind() == Location::Kind::MEM) {
            stack_args += align_up<size_t>(sizeof_type(type, arch::x86_64), 8);
         }

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
   CXIndex index = clang_createIndex(0, 0);
   std::ostream& os;
   Symbols symbols;
   Symbols ignore_structs;
   bool force_all;

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
      
      ABIConversion conv(c);
      conv.emit(os, symbols, ignore_structs);
   }

   void handle_asm_label_attr(CXCursor c, CXCursor p) {
      const std::string sym = to_string(c);
      if (sym.find('$') == std::string::npos) {
         return; /* this is something else */
      }
      
      ABIConversion conv(clang_getCursorType(p), to_string(c));
      conv.emit(os, symbols, ignore_structs);
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
                      "";
                   fprintf(f, usage, argv[0]);
                };

   const char *outpath = nullptr;
   const char *sympath = nullptr;
   const char *symignorepath = nullptr;
   const char *structpath = nullptr;
   const char *optstring = "ho:s:i:r:";
   const struct option longopts[] = {{"help", no_argument, nullptr, 'h'},
                                     {"output", required_argument, nullptr, 'o'},
                                     {"symfile", required_argument, nullptr, 's'},
                                     {"ignorefile", required_argument, nullptr, 'i'},
                                     {"structfile", required_argument, nullptr, 'r'},
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

   auto sym_insert_op = [&] (const std::string& s) { abigen.symbols.insert(s); };
   auto sym_erase_op = [&] (const std::string& s) { abigen.symbols.erase(s); };
   
   if (sympath) {
      parse_syms(sympath, sym_insert_op);
   } else {
      parse_syms(std::cin, sym_insert_op);
   }
   
   if (symignorepath) {
      parse_syms(symignorepath, sym_erase_op);
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
