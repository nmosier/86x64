#pragma once

#include <cstdio>

#include "util.hh"

namespace MachO {

   class Image {
   public:
      Image(const char *path, int mode);
      ~Image();

      std::size_t size() const { return filesize; }
      
      template <typename T>
      const T& at(std::size_t index) const { return * (const T *) ((const char *) img + index); }

      template <typename T>
      T& at(std::size_t index) { return * (T *) ((char *) img + index); }

      void resize(std::size_t newsize);

      Image(const Image&) = delete;

   private:
      int fd;
      int prot;
      std::size_t filesize;
      void *img;

      std::size_t mapsize() const { return std::max(filesize, PAGESIZE); }
   };

}
