#include <iostream>
#include <getopt.h>
#include <list>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unistd.h>
#include <vector>
#include <clang-c/Index.h>

#include "typeinfo.hh"
#include "typeconv.hh"

#if 0
template <typename Func>
void for_each(CXCursor root, Func func) {
   clang_visitChildren(root,
                       [] (CXCursor c, CXCursor parent, CXClientData client_data) {
                          Func func = * (Func *) client_data;
                          return func(c, parent);
                       },
                       &func);
}

template <typename Func>
void for_each(const CXTranslationUnit& unit, Func func) {
   for_each(clang_getTranslationUnitCursor(unit), func);
}
#endif

struct ParseInfo {
   using Syms = std::unordered_set<std::string>;

   typedef CXChildVisitResult (ParseInfo::*op_f)(CXCursor c, CXCursor p);

   CXIndex index = clang_createIndex(0, 0);
   std::list<op_f> ops;
   bool print_types = false;
   bool print_type_kinds = false;
   bool use_canonical_types = false;
   bool arch_i386 = false;
   bool print_sizeof_types = false;

   ~ParseInfo() {
      clang_disposeIndex(index);
   }

   void print_type(CXType t) const {
      if (use_canonical_types) {
         t = clang_getCanonicalType(t);
      }
      std::cout << to_string(t);
   }

   void print_type_kind(CXType t) const {
      if (use_canonical_types) {
         t = clang_getCanonicalType(t);
      }
      std::cout << to_string(t.kind);
   }

   void print_sizeof_type(CXType t) const {
      t = clang_getCanonicalType(t);
      std::cout << sizeof_type(t, arch::x86_64);
   }

   void handle_header(const char *path) {
      std::vector<const char *> args;
      if (arch_i386) {
         args.push_back("-m32");
      }
      args.push_back(nullptr);
      CXTranslationUnit unit = clang_parseTranslationUnit(index, path, args.data(), args.size() - 1,
                                                          nullptr, 0, CXTranslationUnit_None);
      if (unit == nullptr) {
         std::cerr << "Unable to parse translation unit. Quitting." << std::endl;
         exit(-1);
      }

      for (op_f op : ops) {
         for_each(unit, [&] (CXCursor c, CXCursor p) { return (this->*op)(c, p); });
      }
      
      clang_disposeTranslationUnit(unit);
   }

   CXChildVisitResult print_function_decls(CXCursor c, CXCursor p) {
      if (clang_getCursorKind(c) == CXCursor_FunctionDecl) {
         std::cout << to_string(c);
         if (print_type_kinds) {
            std::cout << " ";
            print_type_kind(clang_getCursorType(c));
         }
         if (print_types) {
            std::cout << " ";
            print_type(clang_getCursorType(c));
         }
         if (print_sizeof_types) {
            std::cout << " ";
            print_sizeof_type(clang_getCursorType(c));
         }
      }
      std::cout << std::endl;
      return CXChildVisit_Recurse;
   }

   CXChildVisitResult print_cursors(CXCursor c, CXCursor p) {
      std::cout << "(" << to_string(c) << "," << to_string(clang_getCursorKind(c)) << ")";
      if (print_types) {
         CXType type = clang_getCursorType(c);
         if (print_type_kinds) {
            std::cout << " ";
            print_type_kind(clang_getCursorType(c));
         }
         if (print_types) {
            std::cout << " ";
            print_type(clang_getCursorType(c));
         }
         if (print_sizeof_types) {
            std::cout << " ";
            print_sizeof_type(clang_getCursorType(c));
         }
      }
      std::cout << std::endl;
      return CXChildVisit_Recurse;
   }

   CXChildVisitResult print_asm_labels(CXCursor c, CXCursor p) {
      if (clang_getCursorKind(c) == CXCursor_AsmLabelAttr) {
         std::cout << to_string(c) << " -> " << "(" << to_string(p) << ","
                   << to_string(clang_getCursorKind(p)) << ")" << std::endl;
      }
      return CXChildVisit_Recurse;
   }
};

int main(int argc, char *argv[]) {
   auto usage = [=] (FILE *f) {
                   const char *usage = "usage: %s [-h] [--function-decls|--cursors|--asm-labels|--types|--type-kinds|--canonical-types|--32|--sizeof]* [header...]\n";
                   fprintf(f, usage, argv[0]);
                };

   const char *outpath = nullptr;
   ParseInfo info;
   
   const char *optstring = "ht";
   enum {FUNCTION_DECLS = 256, CURSORS, ASM_LABELS, TYPE_KINDS, CANONICAL_TYPES, ARCH_I386,
   SIZEOF_TYPES};
   const struct option longopts[] =
      {{"function-decls", no_argument, nullptr, FUNCTION_DECLS},
       {"help", no_argument, nullptr, 'h'},
       {"cursors", no_argument, nullptr, CURSORS},
       {"asm-labels", no_argument, nullptr, ASM_LABELS},
       {"types", no_argument, nullptr, 't'},
       {"type-kinds", no_argument, nullptr, TYPE_KINDS},
       {"canonical-types", no_argument, nullptr, CANONICAL_TYPES},
       {"32", no_argument, nullptr, ARCH_I386},
       {"sizeof-types", no_argument, nullptr, SIZEOF_TYPES},
       {0}};
   int optchar;
   while ((optchar = getopt_long(argc, argv, optstring, longopts, nullptr)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout);
         return 0;
      case FUNCTION_DECLS:
         info.ops.push_back(&ParseInfo::print_function_decls);
         break;
      case CURSORS:
         info.ops.push_back(&ParseInfo::print_cursors);
         break;
      case ASM_LABELS:
         info.ops.push_back(&ParseInfo::print_asm_labels);
         break;
      case 't':
         info.print_types = true;
         break;
      case TYPE_KINDS:
         info.print_type_kinds = true;
         break;
      case CANONICAL_TYPES:
         info.use_canonical_types = true;
         break;
      case ARCH_I386:
         info.arch_i386 = true;
         break;
      case SIZEOF_TYPES:
         info.print_sizeof_types = true;
         break;
      case '?':
         usage(stderr);
         return 1;
      }
   }

   /* handle each header */
   for (int i = optind; i < argc; ++i) {
      info.handle_header(argv[i]);
   }

   return 0;
}
