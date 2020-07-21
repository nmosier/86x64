#include "build.hh"
#include "util.hh"

namespace MachO {

   template <Bits bits>
   std::size_t BuildEnv<bits>::allocate(std::size_t size) {
      std::size_t r_offset = loc.offset;
      loc.offset += size;
      return r_offset;
   }
   
   template <Bits bits>
   void BuildEnv<bits>::allocate(std::size_t size, Location& out_loc) {
      
      out_loc.offset = allocate(size);
      out_loc.vmaddr = loc.vmaddr;
      loc.vmaddr += size;
   }

   template <Bits bits>
   void BuildEnv<bits>::newsegment() {
      loc.offset = align_up(loc.offset, PAGESIZE);
      loc.vmaddr = align_up(loc.vmaddr, PAGESIZE);
   }

   template <Bits bits>
   uint8_t BuildEnv<bits>::section_counter() {
      if (section_counter_ == MAX_SECT) {
         throw error("too many sections");
      }
      return ++section_counter_;
   }

   template <Bits bits>
   uint32_t BuildEnv<bits>::lazy_bind_index(uint32_t size) {
      auto tmp = lazy_bind_index_;
      lazy_bind_index_ += size;
      return tmp;
   }

   template class BuildEnv<Bits::M32>;
   template class BuildEnv<Bits::M64>;
   
}
