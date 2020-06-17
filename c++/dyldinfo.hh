#pragma once

#include <mach-o/loader.h>
#include <vector>

#include "macho-fwd.hh"
#include "lc.hh"

namespace MachO {

   template <Bits bits>
   class DyldInfo: public LoadCommand<bits> {
   public:
      dyld_info_command dyld_info;
      std::vector<uint8_t> rebase;
      std::vector<uint8_t> bind;
      std::vector<uint8_t> weak_bind;
      std::vector<uint8_t> lazy_bind;
      std::vector<uint8_t> export_info;

      virtual uint32_t cmd() const override { return dyld_info.cmd; }
      virtual std::size_t size() const override { return sizeof(dyld_info); }

      static DyldInfo<bits> *Parse(const Image& img, std::size_t offset);
      
   private:
      DyldInfo() {}
   };   
   
}
