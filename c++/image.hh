#pragma once

#include <cstdio>
#include <unistd.h>

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
      T& at(std::size_t index) {
         const std::size_t end = index + sizeof(T);
         if (end > filesize) {
            filesize = end;
            if (ftruncate(fd, filesize) < 0) { throw cerror("ftruncate"); }
            if (filesize > mapsize) {
               resize(filesize * 2);
            }
         }
         return * (T *) ((char *) img + index);
      }

      template <typename T>
      void copy(std::size_t offset, const T *begin, std::size_t count) {
         const std::size_t endoff = offset + count * sizeof(T);
         if (endoff > filesize) {
            filesize = endoff;
            if (ftruncate(fd, filesize) < 0) { throw cerror("ftruncate"); }
            if (filesize > mapsize) {
               resize(filesize * 2);
            }
         }
      }
      
      Image(const Image&) = delete;

   private:
      int fd;
      int prot;
      std::size_t filesize;
      std::size_t mapsize;
      void *img;
 
      void resize(std::size_t newsize);
   };

}
