#pragma once

#include <string>

#include "util.hh"
#include "image.hh"
#include "loc.hh"

namespace MachO {


   void init();

   class MachO {
   public:
      virtual uint32_t magic() const = 0;
      virtual uint32_t& magic() = 0;
      virtual Bits bits() const = 0;
      
      static MachO *Parse(const Image& img);
      virtual void Build() = 0;
      virtual void Emit(Image& img) const = 0;
      virtual ~MachO() {}
   };
}

