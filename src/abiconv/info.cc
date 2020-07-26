#include <iostream>
#include <getopt.h>
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

struct ParseInfo {
   using Syms = std::unordered_set<std::string>;

   typedef CXChildVisitResult (ParseInfo::*op_f)(CXCursor c, CXCursor p);

   CXIndex index = clang_createIndex(0, 0);
   std::list<op_f> ops;

   ~ParseInfo() {
      clang_disposeIndex(index);
   }

   void handle_header(const char *path) {
      CXTranslationUnit unit = clang_parseTranslationUnit(index, path, nullptr, 0, nullptr,
                                                          0, CXTranslationUnit_None);
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
         std::cout << to_string(c) << std::endl;
      }
      return CXChildVisit_Recurse;
   }

   CXChildVisitResult print_cursors(CXCursor c, CXCursor p) {
      std::cout << "(" << to_string(c) << "," << to_string(clang_getCursorKind(c)) << ")"
                << std::endl;
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
                   const char *usage = "usage: %s [-h] [--function-decls] [--cursors] [--asm-labels] [header...]\n";
                   fprintf(f, usage, argv[0]);
                };

   const char *outpath = nullptr;
   ParseInfo info;
   
   const char *optstring = "h";
   enum {FUNCTION_DECLS = 256, CURSORS, ASM_LABELS};
   const struct option longopts[] =
      {{"function-decls", no_argument, nullptr, FUNCTION_DECLS},
       {"help", no_argument, nullptr, 'h'},
       {"cursors", no_argument, nullptr, CURSORS},
       {"asm-labels", no_argument, nullptr, ASM_LABELS},
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
