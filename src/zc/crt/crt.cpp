#include "crt.hpp"
#include "asm.hpp"

namespace zc {

   using namespace zc;

   const RegisterValue *crt_arg1(const Value *val) { return crt_arg1(val->size()); }
   const RegisterValue *crt_arg1(int bytes) {
      switch (bytes) {
      case byte_size: return &rv_a;
      case word_size: return &rv_hl;
      case long_size: return &rv_hl;
      default:        abort();
      }
   }

   const RegisterValue *crt_arg2(const Value *val) { return crt_arg2(val->size()); }   
   const RegisterValue *crt_arg2(int bytes) {
      switch (bytes) {
      case byte_size: return &rv_b;
      case word_size: return &rv_bc;
      case long_size: return &rv_bc;
      default:        abort();
      }
   }

   std::string crt_prefix(const Value *val) { return crt_prefix(val->size()); }
   std::string crt_prefix(int bytes) {
      switch (bytes) {
      case byte_size: return "__b";
      case word_size: return "__s";
      case long_size: return "__i";
      default:        abort();
      }
   }

   std::string crt_suffix(const Value *val) { return crt_suffix(val->size()); }
   std::string crt_suffix(bool is_signed) {
      return is_signed ? "s" : "u";
   }
   
}
