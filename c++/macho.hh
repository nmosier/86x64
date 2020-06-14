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

   template <Bits bits>
   class Types {
   public:
      using addr_t = select_type<bits, uint32_t, uint64_t>;
      using size_t = addr_t;
      using mach_header_t = select_type<bits, struct mach_header, struct mach_header_64>;
   };

   using magic_t = uint32_t;

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

      magic_t magic() const { return img->at<magic_t>(0); }
      
   private:
      std::shared_ptr<Image> img;

      void expand(std::size_t offset, std::size_t len);
   };

   template <Bits bits>
   class MachO_: public Types<bits> {
   public:
      using header_t = select_type<bits, struct mach_header, struct mach_header_64>;
      
      MachO_(std::shared_ptr<Image> img): img(img) {}

      friend MachO;

      std::list<LoadCommand<bits>> load_commands() const;
      
   private:
      std::shared_ptr<Image> img;

      void expand(size_t offset, size_t len);

      // TODO: load commands
   };

   template <Bits bits>
   class MachHeader: public Types<bits> {
   public:
      const mach_header_t& operator()() const { return header; }

   private:
      std::shared_ptr<Image> img;
      mach_header_t& header;

      MachHeader(std::shared_ptr<Image> img, size_t offset):
         img(img), header(img->at<mach_header_t>(offset)) {}

      mach_header_t& operator()() { return header; }
   };

   template <Bits bits>
   class LoadCommand: public Types<bits> {
   public:
      using cmd_t = uint32_t;
      using cmdsize_t = uint32_t;

      cmd_t cmd() const { return lc.cmd; }
      cmdsize_t cmdsize() const { return lc.cmdsize; }
      
   private:
      std::shared_ptr<Image> img;
      size_t offset;
      struct load_command& lc;

      LoadCommand(std::shared_ptr<Image> img, size_t offset):
         img(img), offset(offset), lc(img->at<load_command>(offset)) {}
      
      void expand(size_t offset, size_t len);
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

