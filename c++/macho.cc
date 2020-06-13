#include <iostream>

#include <fcntl.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include "macho.hh"
#include "util.hh"

int main(void) {
   
   return 0;
}

namespace MachO {
   
   MachO::MachO(const char *path) {
      if ((fd = open(path, O_RDWR)) < 0) {
         throw cerror("open");
      }

      struct stat st;
      if (fstat(fd, &st) < 0) {
         throw cerror("fstat");
      }

      
   }
   
   MachO::~MachO() {
      
   }

}
