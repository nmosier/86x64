#include <stdint.h>
#include <stdlib.h>
#include <stdexcept>

typedef uint32_t ptr32_t;
typedef uint64_t ptr64_t;

enum class reg_width_t {B, W, D, Q};

namespace {

   template <typename T>
   constexpr T div_up(T a, T b) { return (a + b - 1) / b; }

   template <typename T>
   constexpr T align_up(T a, T b) { return div_up(a, b) * b; }

   template <typename T>
   constexpr reg_width_t type_reg_width() {
      static_assert((sizeof(T) & (sizeof(T) - 1)) == 0);
      int i = 0;
      size_t size = sizeof(T);
      while (size > 1) {
         size >>= 1;
         ++i;
      }
      return (reg_width_t) i;
   }

   template <typename T32, typename T64>
   T64 convert_arg(const void *& args32, void *& args64, reg_width_t *& argtypes) {
      const T64 result = * (T64 *) args64 = * (T32 *) args32;
      args32 = (const char *) args32 + align_up<size_t>(sizeof(T32), 4);
      args64 = (char *) args64 + align_up<size_t>(sizeof(T64), 8);
      *argtypes++ = type_reg_width<T64>();
      return result;
   }

   void printf_parse_directive(const void *& args32, void *& args64, reg_width_t *& argtypes,
                               const char *format) {
      switch (*format++) {
      case 'd':
      case 'i':
      case 'o':
      case 'u':
      case 'x':
      case 'X':
      case 'c':
         convert_arg<int32_t, int32_t>(args32, args64, argtypes);
         break;

      case 'D':
      case 'O':
      case 'U':
         convert_arg<int32_t, int64_t>(args32, args64, argtypes);
         break;

      case 's':
      case 'p':
         convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes);
         break;

      case '%':
         break;
         
      default:
         throw std::invalid_argument("bad format directive character");
      }
   }
   
}

extern "C" int printf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned idx_64 = 0;
   unsigned idx_32 = 0;
   
   const char *format = (const char *) convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes);

   char c;
   while ((c = *format++)) {
      switch (c) {
      case '%':
         printf_parse_directive(args32, args64, argtypes, format);
         break;
         
      default:
         break;
      }
   }

   return 0;
   
}
