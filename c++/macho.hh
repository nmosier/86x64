#pragma once

#include <memory>
#include <list>

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <mach-o/loader.h>

#include "macho-fwd.hh"
#include "util.hh"

namespace MachO {

#if 0
   template <Bits bits>
   class Types {
   public:
      using addr_t = select_type<bits, uint32_t, uint64_t>;
      using size_t = addr_t;
      using mach_header_t = select_type<bits, struct mach_header, struct mach_header_64>;
   };
#endif
   template <Bits bits> using macho_addr_t = select_type<bits, uint32_t, uint64_t>;
   template <Bits bits> using macho_size_t = macho_addr_t<bits>;
   template <Bits bits> using mach_header_t = select_type<bits, mach_header, mach_header_64>;

   class Image {
   public:
      Image(const char *path);
      ~Image();

      template <typename T>
      const T& at(std::size_t index) const { return * (const T *) ((const char *) img + index); }

      template <typename T>
      T& at(std::size_t index) { return * (T *) ((char *) img + index); }

      template <typename T>
      void insert_at(const T& val, std::size_t index) {
         insert_at(&val, 1, index);
      }

      template <typename T>
      void insert_at(const T *val, std::size_t nitems, std::size_t index) {
         resize(filesize + sizeof(*val) * nitems);
         memmove((char *) img + index + sizeof(*val) * nitems, (char *) img + index,
                 filesize - index);
         memcpy((char *) img + index, val, sizeof(*val) * nitems);
      }

      Image(const Image&) = delete;

   private:
      int fd;
      std::size_t filesize;
      void *img;

      void resize(std::size_t newsize);
   };
   
   class MachO {
   public:
      MachO(const char *path): img(std::make_shared<Image>(path)) {}

      void insert_raw(const void *buf, std::size_t buflen, std::size_t offset);

      uint32_t magic() const { return img->at<uint32_t>(0); }
      
   private:
      std::shared_ptr<Image> img;
      std::size_t offset;

      void expand(std::size_t offset, std::size_t len);
   };

   template <Bits bits>
   class MachO_ {
   public:
      MachHeader<bits> header() { return MachHeader<bits>(img, offset); }
      std::list<LoadCommand<bits>> load_commands();

      friend MachO;

   private:
      std::shared_ptr<Image> img;
      macho_size_t<bits> offset;

      MachO_(std::shared_ptr<Image> img, macho_size_t<bits> offset): img(img), offset(offset) {}

      void expand(macho_size_t<bits> offset, macho_size_t<bits> len);
   };

   template <Bits bits>
   class MachHeader {
   public:
      const mach_header_t<bits>& operator()() const { return header; }

      friend MachO_<bits>;
      
   private:
      std::shared_ptr<Image> img;
      mach_header_t<bits>& header;

      MachHeader(std::shared_ptr<Image> img, macho_size_t<bits> offset):
         img(img), header(img->at<mach_header_t<bits>>(offset)) {}

      mach_header_t<bits>& operator()() { return header; }
   };

   template <Bits bits>
   class LoadCommand {
   public:
      uint32_t cmd() const { return lc.cmd; }
      uint32_t cmdsize() const { return lc.cmdsize; }
      
   private:
      std::shared_ptr<Image> img;
      macho_size_t<bits> offset;
      struct load_command& lc;

      LoadCommand(std::shared_ptr<Image> img, macho_size_t<bits> offset):
         img(img), offset(offset), lc(img->at<load_command>(offset)) {}
      
      void expand(macho_size_t<bits> offset, macho_size_t<bits> len);
   };
   
   class Fat {
   public:
      Fat(std::shared_ptr<Image> img): img(img) {}

      friend MachO;
      
   private:
      std::shared_ptr<Image> img;

      void expand(std::size_t offset, std::size_t len); // TODO
   };
   
}

