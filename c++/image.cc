#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "image.hh"
#include "util.hh"

namespace MachO {

   Image::Image(const char *path) {
      if ((fd = open(path, O_RDWR)) < 0) {
         throw cerror("open");
      }

      struct stat st;
      if (fstat(fd, &st) < 0) {
         close(fd);
         throw cerror("fstat");
      }
      filesize = st.st_size;
      
      if ((img = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == NULL) {
         close(fd);
         throw cerror("mmap");
      }
   }
   
   Image::~Image() {
      munmap(img, filesize);
      close(fd);
   }

   void Image::resize(std::size_t newsize) {
      if (munmap(img, filesize) < 0) {
         img = NULL;
         throw cerror("munmap");
      }

      if (ftruncate(fd, newsize) < 0) {
         throw cerror("ftruncate");
      }

      filesize = newsize;

      if ((img = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == NULL) {
         close(fd);
         throw cerror("mmap");
      }

   }   
   
}
