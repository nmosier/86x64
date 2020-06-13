#include <exception>

#include <sys/errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.hh"

namespace MachO {

   cerror::cerror(const char *s) {
      asprintf(&errstr, "%s: %s\n", s, strerror(errno));
   }

   const char *cerror::what() const throw() {
      return errstr;
   }

   cerror::~cerror() {
      free(errstr);
   }
   
}

