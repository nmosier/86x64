#pragma once

#include <cstdio>

namespace MachO {

   class Image {
   public:
      Image(const char *path);
      ~Image();
      
      std::size_t size() const { return filesize; }
      
      template <typename T>
      const T& at(std::size_t index) const { return * (const T *) ((const char *) img + index); }

      template <typename T>
      T& at(std::size_t index) { return * (T *) ((char *) img + index); }

      Image(const Image&) = delete;

   private:
      int fd;
      std::size_t filesize;
      void *img;

      void resize(std::size_t newsize);
   };

}
