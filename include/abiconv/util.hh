#pragma once

#include <cassert>

template <typename Val, typename... Args>
inline void assert_msg(Val val, Args&&... args) {
   if (!val) {
      fprintf(stderr, args...);
      abort();
   }
}
