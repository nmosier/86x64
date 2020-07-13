#pragma once

#include <cstdio>
#include <type_traits>

#include "resolve.hh"
#include "types.hh"
#include "util.hh"

namespace MachO {

   template <Bits b1, Bits b2>
   class TransformEnv {
   public:
      template <template<Bits> typename T>
      void add(const T<b1> *key, T<b2> *pointee) {
         if (key) {
            resolver.add(key, pointee);
         } else {
            fprintf(stderr, "warning: %s: not adding null key\n", __FUNCTION__);
         }
      }
      template <template<Bits> typename T>
      void resolve(const T<b1> *key, const T<b2> **pointer) {
         if (key) {
            resolver.resolve(key, (const void **) pointer);
         } else {
            fprintf(stderr, "warning: %s: not resolving null key\n", __FUNCTION__);
         }
      }

      void operator()(const mach_header_t<b1>& h1, mach_header_t<b2>& h2) const;
      void operator()(const segment_command_t<b1>& h1, segment_command_t<b2>& h2) const;
      void operator()(const nlist_t<b1>& n1, nlist_t<b2>& n2) const;
      void operator()(const section_t<b1>& s1, section_t<b2>& s2) const;
      
   private:
      Resolver<const void *, void, false> resolver;
   };
   
}
