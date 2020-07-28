#pragma once

#include <cassert>
#include <cstdio>
#include <iostream>

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
constexpr T align_up(T n, T align) {
   return div_up(n, align) * align;
}
