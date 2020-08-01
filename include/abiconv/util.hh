#pragma once

#include <cassert>
#include <cstdio>
#include <iostream>
#include <clang-c/Index.h>

template <typename Val, typename... Args>
inline void assert_msg(Val val, Args&&... args) {
   if (!val) {
      fprintf(stderr, args...);
      abort();
   }
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

inline std::ostream& strjoin(std::ostream& os, const std::string& separator) {
   return os;
}

template <typename T>
constexpr T div_up(T a, T b) {
   return (a + b - 1) / b;
}

template <typename T>
constexpr T div_down(T a, T b) {
   return a / b;
}

template <typename T>
constexpr T align_up(T n, T align) {
   if (n >= 0) {
      return div_up(n, align) * align;
   } else {
      return -div_down(-n, align) * align;
   }
}

template <typename Func>
static void for_each(CXCursor root, Func func) {
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

std::string to_string(CXType type);
std::string to_string(CXCursorKind kind);
std::string to_string(CXCursor cursor);
std::string to_string(CXTypeKind kind);

