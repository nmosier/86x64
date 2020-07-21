#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <sys/stat.h>
#include <sys/mman.h>

#include "image.hh"
#include "util.hh"
#include "types.hh"

namespace MachO {

   Image::Image(const char *path, int mode) {
      int mode2 = 0;
      if ((mode & O_CREAT)) {
         mode2 = 0776;
      }
      
      if ((fd = open(path, mode, mode2)) < 0) {
         throw cerror("open");
      }

      struct stat st;
      if (fstat(fd, &st) < 0) {
         close(fd);
         throw cerror("fstat");
      }
      filesize = st.st_size;
      mapsize = std::max<std::size_t>(filesize, getpagesize());

      if ((mode & O_RDWR)) {
         prot = PROT_READ | PROT_WRITE;
      } else {
         prot = PROT_READ;
      }
      
      if ((img = mmap(NULL, mapsize, prot, MAP_SHARED, fd, 0)) == MAP_FAILED) {
         close(fd);
         throw cerror("mmap");
      }
   }
   
   Image::~Image() {
      munmap(img, mapsize);
      ftruncate(fd, filesize);
      close(fd);
   }

   void Image::resize(std::size_t newsize) {
      if (munmap(img, mapsize) < 0) {
         img = NULL;
         throw cerror("munmap");
      }

      if (ftruncate(fd, newsize) < 0) {
         throw cerror("ftruncate");
      }

      mapsize = newsize;
      
      if ((img = mmap(NULL, mapsize, prot, MAP_SHARED, fd, 0)) == MAP_FAILED) {
         close(fd);
         throw cerror("mmap");
      }

   }

   void Image::grow(std::size_t size) {
      if (size > filesize) {
         filesize = size;
         if (ftruncate(fd, filesize) < 0) { throw cerror("ftruncate"); } 
         if (filesize > mapsize) {
            resize(filesize * 2);
         }
      }
   }

   void Image::memset(std::size_t offset, int c, std::size_t bytes) {
      grow(offset + bytes);
      ::memset((char *) img + offset, c, bytes);
   }
   
}
