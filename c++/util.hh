#pragma once

#include "macho-fwd.hh"

namespace MachO {

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

}
