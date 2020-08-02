#include "util.hh"

namespace {
   template <typename CXT, typename Func>
   std::string to_string_aux(CXT val, Func func) {
      CXString cxs = func(val);
      std::string str = clang_getCString(cxs);
      clang_disposeString(cxs);
      return str;
   }
}

std::string to_string(CXType type) { return to_string_aux(type, clang_getTypeSpelling); }
std::string to_string(CXCursorKind kind) {
   return to_string_aux(kind, clang_getCursorKindSpelling);
}
std::string to_string(CXCursor cursor) { return to_string_aux(cursor, clang_getCursorSpelling); }
std::string to_string(CXTypeKind kind) { return to_string_aux(kind, clang_getTypeKindSpelling) ;}

