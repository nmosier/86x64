#pragma once

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <mach-o/loader.h>

#include "macho-fwd.hh"
#include "util.hh"

namespace MachO {

   template <Bits bits>
   using macho_addr_t = select_type<bits, uint32_t, uint64_t>;
   template <Bits bits>
   using macho_size_t = macho_addr_t<bits>;

   class Image {
   public:
      Image(const char *path);
      ~Image();

      template <typename T>
      const T& at(size_t index) const { return * (const T *) ((const char *) img + index); }

      template <typename T>
      T& at(size_t index) { return * (T *) ((char *) img + index); }

      template <typename T>
      void insert_at(const T& val, size_t index) {
         insert_at(&val, 1, index);
      }

      template <typename T>
      void insert_at(const T *val, size_t nitems, size_t index) {
         resize(filesize + sizeof(*val) * nitems);
         memmove((char *) img + index + sizeof(*val) * nitems, (char *) img + index,
                 filesize - index);
         memcpy((char *) img + index, val, sizeof(*val) * nitems);
      }

   private:
      int fd;
      size_t filesize;
      void *img;

      void resize(size_t newsize);
   };
   
   class MachO {
   public:
      MachO(const char *path): img(path) {}
      
   private:
      Image img;
   };
   
}

