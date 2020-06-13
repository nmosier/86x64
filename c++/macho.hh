#pragma once

#include <stdint.h>
#include <fcntl.h>
#include <mach-o/loader.h>

#include "macho-fwd.hh"
#include "util.hh"

namespace MachO {

   template <Bits bits>
   using macho_addr_t = select_type<bits, uint32_t, uint64_t>;
   template <Bits bits>
   using macho_size_t = macho_addr_t<bits>;

#if 0
   class Image {
   public:
      Image(const char *path, int oflag) {
         if ((fd = open(path, O_RDONLY)))
      }
      
      
   private:
      int fd;
      void *map;
      // size_t size;
   };
   
   class MachO {
   public:
      using magic_t = uint32_t;
      
      virtual magic_t magic() const = 0;
   protected:
      MachO() {}
   };

   class Fat: public MachO {
   public:
      
   private:
      
   };

   template <Bits bits>
   class Archive: public MachO {
   public:
      using filetype_t = uint32_t;
      using flags_t = uint32_t;
      
      virtual magic_t magic() const override { return magic_; }
      cpu_type_t cputype() const { return cputype_; }
      cpu_subtype_t cpusubtype() const { return cpusubtype_; }
      filetype_t filetype() const { return filetype_; }
      flags_t flags() const { return flags_; }

   protected:
      magic_t magic_;
      cpu_type_t cputype_;
      cpu_subtype_t cpusubtype_;
      filetype_t filetype_;
      flags_t flags_;

      Archive(magic_t magic, cpu_type_t cputype, cpu_subtype_t cpusubtype, filetype_t filetype,
              flags_t flags): magic_(magic), cputype_(cputype), cpusubtype_(cpusubtype),
                              filetype_(filetype), flags_(flags) {}
   };

   template <Bits bits>
   class MachHeader {
   public:
      uint32_t magic() const { return header.magic; }
      
   private:
      using Header = select_type<bits, struct mach_header, struct mach_header_64>;
      Header header;
   };

#endif


   class MachO {
   public:
      MachO(const char *path);
      ~MachO();
      
   private:
      int fd;
      void *img;
   };
   
}

