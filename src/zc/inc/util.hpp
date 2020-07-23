#ifndef __UTIL_HPP
#define __UTIL_HPP

#include <variant>
#include <iostream>
#include <vector>
#include <unordered_map>

namespace zc {

   inline std::ostream& indent(std::ostream& out, int padding) {
      if (padding > 80)
         padding = 80;
      else if (padding < 0)
         return out;
      std::fill_n(std::ostream_iterator<char>(out), padding, ' ');
      return out;
   }


   /*** VISITATION & VARIANTS ***/
   template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
   template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;


   /*** VEC TO TUPLE ***/
   template <class InputIt, typename... Ts>
   struct vec_to_tupler {
      template <int I>
      static void do_it(std::optional<std::tuple<Ts...>>& t, InputIt begin, InputIt end) {
         if (begin == end) {
            t = std::nullopt;
         } else {
            auto *ptr = dynamic_cast
               <typename std::remove_reference<decltype(std::get<I-1>(*t))>::type>(*begin);
            if (ptr) {
               std::get<I-1>(*t) = ptr;
               do_it<I-1>(t, ++begin, end);
            } else {
               t = std::nullopt;
            }
         }
      }

      template <>
      static void do_it<0>(std::optional<std::tuple<Ts...>>& t, InputIt begin, InputIt end) {
         if (begin != end) {
            t = std::nullopt;
         }
      }
   };

   template <class InputIt, typename... Ts>
   std::optional<std::tuple<Ts...>> vec_to_tuple(InputIt begin, InputIt end) {
      std::optional<std::tuple<Ts...>> t;
      vec_to_tupler<InputIt, Ts...>::template
         do_it<std::tuple_size<std::tuple<Ts...>>::value>(t, begin, end);
      return t;
   }


   /*** PORTAL ***/
   template <typename T>
   struct portal {
      void send(const T& val) const { *std::get<T*>(v) = val; }

      T& get() { return std::get<T>(v); }
      const T& get() const { return std::get<T>(v); }

      operator bool() const { return std::holds_alternative<T>(v); }
      
      T& operator*() { return std::get<T>(v); }
      const T& operator*() const { return std::get<T>(v); }

      portal(const T& val): v(val) {}
      portal(T *ptr): v(ptr) {}
      
   private:
      std::variant <T, T*> v;
   };
   
}

#endif
