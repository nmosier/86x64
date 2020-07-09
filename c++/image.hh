#pragma once

#include <cstdio>
#include <unistd.h>

#include "util.hh"

namespace MachO {

   class Image {
   public:
      Image(const char *path, int mode);
      Image() {}
      ~Image();

      std::size_t size() const { return filesize; }
      
      template <typename T>
      const T& at(std::size_t index) const { return * (const T *) ((const char *) img + index); }

      template <typename T>
      T& at(std::size_t index) {
         grow(index + sizeof(T));
         return * (T *) ((char *) img + index);
      }

      template <typename Iterator>
      void copy(std::size_t offset, Iterator begin, std::size_t count) {
         const std::size_t bytes = count * sizeof(*begin);
         grow(offset + bytes);
         memcpy((char *) img + offset, &*begin, bytes);
      }
      
      void memset(std::size_t offset, int c, std::size_t bytes);

      Image(const Image&) = delete;
      
   private:
      int fd;
      int prot;
      std::size_t filesize;
      std::size_t mapsize;
      void *img;
      
      void resize(std::size_t newsize);
      void grow(std::size_t size);
   };

}
