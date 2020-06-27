#pragma once

#include "resolve.hh"

namespace MachO {


   template <Bits b1, Bits b2>
   class TransformEnv {
   public:
      template <template<Bits> typename T>
      void add(const T<b1> *key, const T<b2> *pointee) {
         resolver.add(key, pointee);
      }
      template <template<Bits> typename T>
      void resolve(const T<b1> *key, const T<b2> **pointer) {
         resolver.resolve(key, pointer);
      }
      
   private:
      Resolver<const void *, void> resolver;
   };
   
}
