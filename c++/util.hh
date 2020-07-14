#pragma once

#include <exception>
#include <sys/errno.h>
#include <string.h>
#include <stdio.h>

#include "bits.hh"

namespace MachO {

   struct functor {
      virtual void operator()() = 0;
   };

   struct functor_nop: functor {
      virtual void operator()() override {}
   };

   namespace {
      template <Bits bits, typename T32, typename T64>
      auto select_type_func() {
         if constexpr (bits == Bits::M32) {
               return T32(); 
            } else {
            return T64();
         }
      }
   }

   /**
    * Utility template for selecting using enum to select between two types.
    * @tparam bits the enumeration constant to switch on
    * @tparam type if bits == M32
    * @tparam type if bits == M64
    */
   template <Bits bits, typename T32, typename T64>
   using select_type = decltype(select_type_func<bits,T32,T64>());

   template <typename T>
   constexpr T select_value(Bits bits, T v32, T v64) {
      return bits == Bits::M32 ? v32 : v64;
   }

   class cerror: public std::exception {
   public:
      cerror(const char *s) { asprintf(&errstr, "%s: %s\n", s, strerror(errno)); }
      ~cerror() { free(errstr); }

      virtual const char *what() const throw() { return errstr; }
      
   private:
      char *errstr;
   };

   class error: public std::exception {
   public:
      template <typename... Args>
      error(const char *fmt, Args... args) {
         asprintf(&errstr, fmt, args...);
      }
      ~error() { free(errstr); }

      virtual const char *what() const throw() { return errstr; }
      
   private:
      char *errstr;
   };



   template <typename T>
   constexpr T div_up(T a, T b) {
      static_assert(std::is_integral<T>::value);
      static_assert(std::is_unsigned<T>::value);
      return (a + b - 1) / b;
   }

   template <typename T>
   constexpr T div_down(T a, T b) {
      static_assert(std::is_integral<T>::value);
      static_assert(std::is_unsigned<T>::value);
      return a / b;
   }

   template <typename T>
   constexpr T align_up(T a, T b) {
      return div_up(a, b) * b;
   }

   template <typename T>
   constexpr T align_down(T a, T b) {
      return div_down(a, b) * b;
   }


   namespace {
      template <Bits bits>
      constexpr std::size_t align_bytes = bits == Bits::M32 ? 4 : 8;
   }

   template <Bits bits>
   constexpr std::size_t align(std::size_t value) { return align_up(value, align_bytes<bits>); }

   template <Bits bits>
   constexpr std::size_t align_rem(std::size_t value) { return align<bits>(value) - value; }

   template <Bits bits>
   constexpr Bits opposite = bits == Bits::M32 ? Bits::M64 : Bits::M32;

   #define STR(s) #s
   #define XSTR(s) STR(s)
   
#define case_log(label) case label: fprintf(stderr, xstr(label)  "\n");

   template <typename T>
   constexpr void big_endian(const T& in, T& out) {
      static_assert(std::is_integral<T>());
      const uint8_t *data = (const uint8_t *) &in;
      out = 0;
      for (std::size_t i = 0; i < sizeof(T); ++i, ++data) {
         out <<= 8;
         out += *data;
      }
   }
 
}
