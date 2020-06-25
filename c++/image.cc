#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <sys/stat.h>
#include <sys/mman.h>

#include "image.hh"
#include "util.hh"

namespace MachO {

   Image::Image(const char *path, int mode) {
      if ((fd = open(path, mode)) < 0) {
         throw cerror("open");
      }

      struct stat st;
      if (fstat(fd, &st) < 0) {
         close(fd);
         throw cerror("fstat");
      }
      filesize = st.st_size;
      mapsize = std::max(filesize, PAGESIZE);

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
   
}
