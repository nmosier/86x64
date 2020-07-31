#include <iostream>
#include <list>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <clang-c/Index.h>

#include "util.hh"
#include "abigen.hh"
#include "typeinfo.hh"
#include "typeconv.hh"

bool force_all = false;

const std::string& reg_group::reg(reg_width width) const {
   switch (width) {
   case reg_width::B: return reg_b;
   case reg_width::W: return reg_w;
   case reg_width::D: return reg_d;
   case reg_width::Q: return reg_q;
   default: throw std::invalid_argument("bad register width");
   }
}

const reg_group rax = {"al", "ax", "eax", "rax"};
const reg_group rdi = {"dil", "di",  "edi", "rdi"};
const reg_group rsi = {"sil", "si",  "esi", "rsi"};
const reg_group rdx = {"dl",  "dx",  "edx", "rdx"};
const reg_group rcx = {"cl",  "cx",  "ecx", "rcx"};
const reg_group r8  = {"r8b", "r8w", "r8d", "r8"};
const reg_group r9  = {"r9b", "r9w", "r9d", "r9"};
const reg_group r11 = {"r11b", "r11w", "r11d", "r11"};
const reg_group rsp = {"spl", "sp", "esp", "rsp"};
const reg_group rbp = {"bpl", "bp", "ebp", "rbp"};

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

#if 0
emit_arg::emit_arg(CXType param): param(param), param_width_32(get_type_width(param, arch::i386)),
                                  param_width_64(get_type_width(param, arch::x86_64)),
                                  sign(get_type_signed(param)) {}

std::string emit_arg::memop(const memloc& loc) const {
   std::stringstream src;
   src << reg_width_to_str(param_width_32) << " [" << loc.basereg << " + " << loc.index << "]";
   src << "\t" << "; " << clang_getTypeSpelling(param);
   return src.str();
}

void emit_arg::load(std::ostream& os, memloc& loc) {
   emit_inst(os, opcode(), regstr(), memop(loc));
   loc.index += align_up<unsigned>(reg_width_size(param_width_32), 4);
}

void emit_arg::store(std::ostream& os, memloc& loc) {
   emit_inst(os, opcode(), memop(loc), regstr());
   loc.index += align_up<unsigned>(reg_width_size(param_width_32), 4);
}

const char *emit_arg_int::opcode() {
   if (param_width_32 == param_width_64) {
      return "mov";
   } else if (param_width_32 == reg_width::D && param_width_64 == reg_width::Q && !sign) {
      param_width_64 = param_width_32;
      return "mov";
   } else if (sign) {
      return "movsxd";
   } else {
      return "movzx";
   }
}

const char *emit_arg_real::opcode() {
   switch (param_width_32) {
   case reg_width::D:
      return "movss";
      break;
   case reg_width::Q:
      return "movsd";
      break;
   default: abort();
   }
}
#endif


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

   /* assumes stack is 16-byte aligned */
   size_t stack_args_size() const {
      size_t size = 0;
      unsigned reg_i = 0;
      unsigned xmm_i = 0;
      
      for (unsigned argi = 0; argi < argc(); ++argi) {
         CXType argtype = clang_getCanonicalType(clang_getArgType(function_type, argi));
         switch (get_type_domain(argtype)) {
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

#if 0
      for (unsigned argi = 0; argi < argc(); ++argi) {
         CXType argtype = clang_getCanonicalType(clang_getArgType(function_type, argi));
         if (should_convert_type(argtype)) {
            size += sizeof_type(get_convert_type(argtype), arch::x86_64);
         }
      }
#endif

      return align_up<size_t>(size, 16);
   }

   void emit(std::ostream& os, Symbols& symbols) const {
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
#if 0
      memloc stack_args = {"rsp", 0};
      memloc stack_data = {"rsp", static_cast<int>(stack_args_size())};
#else
      MemoryLocation stack_args(rsp, 0);
      // MemoryLocation stack_data(rsp, stack_args_size());
      conversion conv = {true, arch::i386, arch::x86_64, {rsp, static_cast<int>(stack_args_size())}};
#endif
      
      emit_inst(os, "sub", "rsp", stack_data_size() + stack_args_size());
      
      /* transfer arguments */
      const reg_groups regs = {&rdi, &rsi, &rdx, &rcx, &r8, &r9};
      param_info info(regs, 8);
      int param_it;
      int param_end = clang_getNumArgTypes(function_type);
#if 0
      memloc load_loc = {"rbp", 12};
#else
      MemoryLocation load_loc(rbp, 12);
#endif
      for (param_it = 0;
           param_it != param_end /* && reg_it != regs.end() */;
           ++param_it /*, ++reg_it */) {

         CXType param_type = handle_type(clang_getArgType(function_type, param_it));

#if 1
         std::unique_ptr<Location> dst;
         switch (get_type_domain(param_type)) {
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

         conv.convert(os, param_type, load_loc, *dst);

         load_loc += align_up<size_t>(sizeof_type(param_type, arch::i386), 4);
         if (dst->kind() == Location::Kind::MEM) {
            stack_args += align_up<size_t>(sizeof_type(param_type, arch::x86_64), 8);
         }
#else
         switch (get_type_domain(param_type)) {
         case type_domain::INT:
            {
               memloc arg_data;
               if (info.reg_it != info.reg_end) {
                  arg_data.basereg = (*info.reg_it)->reg_q.c_str();
                  arg_data.index = 0;
                  
                  emit_arg_int(param_type, **info.reg_it++).load(os, load_loc);
               } else {
                  emit_arg_int arg(param_type, rax);
                  arg.load(os, load_loc);
                  arg.store(os, stack_args);

                  arg_data.basereg = "rax";
                  arg_data.index = 0;
               }
               if (do_conv) {
                  memloc data_dst = stack_data;
                  convert_type(os, get_convert_type(param_type), arg_data, stack_data);
                  emit_inst(os, "lea", arg_data.basereg, data_dst.memop());
               }
            }
            break;
            
         case type_domain::REAL:
            assert(!do_conv);
            if (info.fp_idx != info.fp_end) {
               emit_arg_real(param_type, info.fp_idx++).load(os, load_loc);
            } else {
               fprintf(stderr, "xmm depleted\n");
               abort();
            }
            break;
               
         default: abort();
         }
#endif
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
   Symbols symbols;
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
      conv.emit(os, symbols);
   }

   void handle_asm_label_attr(CXCursor c, CXCursor p) {
      const std::string sym = to_string(c);
      if (sym.find('$') == std::string::npos) {
         return; /* this is something else */
      }
      
      ABIConversion conv(clang_getCursorType(p), to_string(c));
      conv.emit(os, symbols);
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

int main(int argc, char *argv[]) {
   auto usage = [=] (FILE *f) {
                   const char *usage = "usage: %s [-h] [-o <outpath>] [-s <symfile>] [-i <ignorefile>] <header>...\n";
                   fprintf(f, usage, argv[0]);
                };

   const char *outpath = nullptr;
   const char *sympath = nullptr;
   const char *symignorepath = nullptr;
   
   const char *optstring = "ho:s:i:";
   int optchar;
   while ((optchar = getopt(argc, argv, optstring)) >= 0) {
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

   abigen.emit_header();
   
   /* handle each header */
   for (int i = optind; i < argc; ++i) {
      abigen.handle_file(argv[i]);
   }

   return 0;
}
