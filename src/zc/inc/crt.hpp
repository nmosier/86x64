#ifndef __CRT_HPP
#define __CRT_HPP

#include "asm-fwd.hpp"

namespace zc {
   
   using namespace z80;
   
   const RegisterValue *crt_arg1(const Value *val);
   const RegisterValue *crt_arg1(int bytes);
   const RegisterValue *crt_arg2(const Value *val);
   const RegisterValue *crt_arg2(int bytes);   
   
   std::string crt_prefix(int bytes);
   std::string crt_prefix(const Value *val);
   std::string crt_suffix(bool is_signed);
   std::string crt_suffix(const Value *val);

}

#endif
