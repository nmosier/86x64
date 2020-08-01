#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <list>
#include <string>
#include <optional>
#include <unordered_map>

typedef uint32_t ptr32_t;
typedef uint64_t ptr64_t;

typedef uint32_t size32_t;
typedef uint64_t size64_t;

typedef int32_t i32_t;
typedef int32_t i64_t;

typedef float f32_t;
typedef float f64_t;

typedef int32_t l32_t;
typedef int64_t l64_t;

typedef int64_t ll32_t;
typedef int64_t ll64_t;

typedef int16_t s32_t;
typedef int16_t s64_t;

typedef signed char sc32_t;
typedef signed char sc64_t;

typedef int64_t j32_t;
typedef int64_t j64_t;

typedef int32_t t32_t;
typedef int64_t t64_t;

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
   T64 convert_arg(const void *& args32, void *& args64, reg_width_t *& argtypes,
                          unsigned& arg_count) {
      const T64 result = * (T64 *) args64 = * (T32 *) args32;
      args32 = (const char *) args32 + align_up<size_t>(sizeof(T32), 4);
      args64 = (char *) args64 + align_up<size_t>(sizeof(T64), 8);
      *argtypes++ = type_reg_width<T64>();
      ++arg_count;
      return result;
   }

   /* convert arg, silently */
   template <typename T32, typename T64>
   void convert_arg_s(const void *& args32, void *& args64, reg_width_t *& argtypes,
                      unsigned& arg_count) {
      convert_arg<T32, T64>(args32, args64, argtypes, arg_count);
   }

   static bool printf_is_length_modifier(char c) {
      switch (c) {
      case 'h':
      case 'l':
      case 'j':
      case 't':
      case 'z':
         return true;
      default:
         return false;
      }
   }

   enum class printf_type {SIGNED, UNSIGNED, STRING, FLOAT, CHAR, POINTER, ESCAPE};
   printf_type printf_parse_type(const char *& format) {
      const std::list<std::pair<std::string, printf_type>> conv =
         {{"di", printf_type::SIGNED},
          {"ouxX", printf_type::UNSIGNED},
          {"eEfFgGaA", printf_type::FLOAT},
          {"c", printf_type::CHAR},
          {"s", printf_type::STRING},
          {"p", printf_type::POINTER},
          {"%", printf_type::ESCAPE},
         };
      for (auto pair : conv) {
         if (pair.first.find(*format) != std::string::npos) {
            ++format;
            return pair.second;
         }
      }

      throw std::invalid_argument("invalid conversion specifier");
   }

   enum class printf_modifier {H, HH, L, LL, J, T, Z};
   std::optional<printf_modifier> printf_parse_modifier(const char *& format) {
      const std::list<std::pair<const char *, printf_modifier>> conv =
         {{"h", printf_modifier::H},
          {"hh", printf_modifier::HH},
          {"l", printf_modifier::L},
          {"ll", printf_modifier::LL},
          {"j", printf_modifier::J},
          {"t", printf_modifier::T},
          {"z", printf_modifier::Z},
         };
      for (auto pair : conv) {
         auto len = strlen(pair.first);
         if (strncmp(pair.first, format, len) == 0) {
            format += len;
            return pair.second;
         }
      }
      return std::nullopt;
   }

   void printf_parse_directive(const void *& args32, void *& args64, reg_width_t *& argtypes,
                               const char *& format, unsigned& arg_count) {
      /* parse flags */
      switch (*format) {
      case '#':
      case '0':
      case '-':
      case ' ':
      case '+':
      case '\'':
         ++format;
         break;
      }

      /* parse minimum field width */
      while (isdigit(*format)) {
         ++format;
      }

      /* parse precision */
      if (*format == '.') {
         while (isdigit(*++format)) {}
      }

      /* parse length modifier */
      std::optional<printf_modifier> modifier = printf_parse_modifier(format);
      
      /* parse format specifier */
      const printf_type type = printf_parse_type(format);

      /* do conversion */
      const std::unordered_map<printf_type,
                               std::unordered_map<std::optional<printf_modifier>,
                                                  void (*)(const void*&, void*&, reg_width_t*&,
                                                           unsigned&)
                                                  >
                               > converter
         {{printf_type::SIGNED, {{std::nullopt, convert_arg_s<i32_t, i64_t>},
                                 {printf_modifier::HH, convert_arg_s<sc32_t, sc64_t>},
                                 {printf_modifier::H, convert_arg_s<s32_t, s64_t>},
                                 {printf_modifier::L, convert_arg_s<l32_t, l64_t>},
                                 {printf_modifier::LL, convert_arg_s<ll32_t, ll64_t>},
                                 {printf_modifier::J, convert_arg_s<j32_t, j64_t>},
                                 {printf_modifier::T, convert_arg_s<t32_t, t64_t>},
                                 {printf_modifier::Z, convert_arg_s<int8_t, int8_t>}
               }
           },
          {printf_type::UNSIGNED, {{std::nullopt, convert_arg_s<i32_t, i64_t>},
                                   {printf_modifier::HH, convert_arg_s<uint8_t, uint8_t>},
                                   {printf_modifier::H, convert_arg_s<uint16_t, uint16_t>},
                                   {printf_modifier::L, convert_arg_s<uint32_t, uint64_t>},
                                   {printf_modifier::LL, convert_arg_s<uint64_t, uint64_t>},
                                   {printf_modifier::J, convert_arg_s<uint64_t, uint64_t>},
                                   {printf_modifier::T, convert_arg_s<uint32_t, uint64_t>},
                                   {printf_modifier::Z, convert_arg_s<uint32_t, uint64_t>}
             }
          },
          {printf_type::STRING, {{std::nullopt, convert_arg_s<uint32_t, uint64_t>}}},
          {printf_type::FLOAT, {{std::nullopt, convert_arg_s<double, double>},
                                {printf_modifier::L, convert_arg_s<double, double>}
             }
          },
          {printf_type::CHAR, {{std::nullopt, convert_arg_s<int32_t, int32_t>}}},
          {printf_type::POINTER, {{std::nullopt, convert_arg_s<uint32_t, uint64_t>}}},
          {printf_type::ESCAPE, {{std::nullopt, nullptr}}}
         };

      converter.at(type).at(modifier)(args32, args64, argtypes, arg_count);

   }
   
}

extern "C" unsigned printf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned idx_64 = 0;
   unsigned idx_32 = 0;
   unsigned arg_count = 0;

   const char *format = (const char *) convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes,
                                                                     arg_count);
   char c;
   while ((c = *format++)) {
      switch (c) {
      case '%':
         printf_parse_directive(args32, args64, argtypes, format, arg_count);
         break;
         
      default:
         break;
      }
   }
   
   return arg_count;
}

extern "C" unsigned sprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned fprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned snprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   convert_arg<size32_t, size64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned asprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   return sprintf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned dprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<i32_t, i64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned __sprintf_chk_conversion_f(const void *args32, void *args64,
                                               reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   convert_arg<i32_t, i64_t>(args32, args64, argtypes, arg_count);
   convert_arg<size32_t, size64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 3;
}
