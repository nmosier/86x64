#pragma once

#include <mach-o/fat.h>

#include "macho.hh"
#include "types.hh"
#include "archive.hh"

namespace MachO {

   template <Bits bits>
   class Fat: public MachO {
   public:
      using ArchiveNode = std::pair<fat_arch_t<bits>, AbstractArchive *>;
      using Archives = std::list<ArchiveNode>;
      
      fat_header header;
      Archives archives;

      virtual uint32_t magic() const override { return header.magic; }
      virtual uint32_t& magic() override { return header.magic; }
      std::size_t size() const {
         return sizeof(header) + sizeof(fat_arch_t<bits>) * archives.size();
      }
      
      static Fat<bits> *Parse(const Image& img) { return new Fat(img); }
      virtual void Build() override;
      virtual void Emit(Image& img) const override;
      
   private:
      Fat(const Image& img);
   };
   
}
