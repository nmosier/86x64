#include <iostream>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <mach-o/fat.h>
#include <mach-o/loader.h>

#include "macho.hh"
#include "util.hh"

int main(int argc, char *argv[]) {
   const char *usage = "usage: %s path\n";

   if (argc != 2) {
      fprintf(stderr, usage, argv[0]);
      return 1;
   }
   
   MachO::Image img(argv[1]);

   img.insert_at('X', 0);
   img.insert_at('X', 2);
   img.insert_at('X', 4);
   img.insert_at('X', 6);
   
   return 0;
}

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

   void Image::resize(size_t newsize) {
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

   void MachO::insert_raw(const void *buf, size_t buflen, size_t offset) {
      // TODO
      throw "not complete";
   }

   void MachO::expand(size_t offset, size_t len) {
      switch (magic()) {
      case MH_MAGIC:
         MachO_<Bits::M32>(img).expand(offset, len);
         break;
         
      case MH_MAGIC_64:
         MachO_<Bits::M64>(img).expand(offset, len);
         break;

      case FAT_MAGIC:
         Fat(img).expand(offset, len);
         break;
         
      case MH_CIGAM:
      case MH_CIGAM_64:
      case FAT_CIGAM:
         throw error("%s: archive is of opposite endianness", __FUNCTION__);

      }
   }

   template <Bits bits>
   void MachO_<bits>::expand(size_t offset, size_t len) {
      const header_t& header = img->at<header_t>(0);

      /* iterate through headers */
      
      // TODO
   }


   void Fat::expand(size_t offset, size_t len) {
      throw error("%s: stub", __FUNCTION__);
   }
   
}
