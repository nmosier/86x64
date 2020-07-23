#ifndef __ASM_MODE_HPP
#define __ASM_MODE_HPP

namespace zc::z80 {


   constexpr bool ez80_mode =
#ifdef Z80
      false
#else
      true
#endif
      ;

   constexpr int flag_size = 0;
   constexpr int byte_size = 1;
   constexpr int word_size = 2;
   constexpr int long_size =
#ifdef Z80
      2
#else
      3
#endif
      ;

   constexpr intmax_t byte_max = (1 << (byte_size * 8)) - 1;
   constexpr intmax_t word_max = (1 << (word_size * 8)) - 1;
   constexpr intmax_t long_max = (1 << (long_size * 8)) - 1;

}

#endif
