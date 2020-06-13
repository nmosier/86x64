#pragma once

#include "macho.hh"

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
   
}
