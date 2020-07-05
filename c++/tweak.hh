#pragma once

#include <mach-o/loader.h>

#include "image.hh"
#include "util.hh"

namespace MachO::Tweak {

   template <Bits bits>
   using mach_header_t = select_type<bits, mach_header, mach_header_64>;
   
   class MachO {
   public:
      uint32_t& magic;

      static MachO *Parse(Image& img);

      virtual void dummy() {}

   protected:
      MachO(Image& img): magic(img.at<uint32_t>(0)) {}
   };

   class AbstractArchive: public MachO {
   public:
   protected:
      AbstractArchive(Image& img): MachO(img) {}
   };

   template <Bits bits>
   class Archive: public AbstractArchive {
   public:
      mach_header_t<bits>& header;

      static Archive<bits> *Parse(Image& img, std::size_t offset = 0) {
         return new Archive(img, offset);
      }
      
   private:
      Archive(Image& img, std::size_t offset):
         AbstractArchive(img), header(img.at<mach_header_t<bits>>(offset)) {}
   };
   
}
