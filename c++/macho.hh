#pragma once

#include "macho-fwd.hh"
#include "util.hh"
#include "image.hh"

namespace MachO {

   template <Bits bits> using macho_addr_t = select_type<bits, uint32_t, uint64_t>;
   template <Bits bits> using macho_size_t = macho_addr_t<bits>;

   void init();

   class MachO {
   public:
      virtual uint32_t magic() const = 0;
      virtual uint32_t& magic() = 0;
      
      static MachO *Parse(const Image& img);
      virtual void Build() = 0;
      virtual void Emit(Image& img) const = 0;
      virtual ~MachO() {}

   private:
   };

}

