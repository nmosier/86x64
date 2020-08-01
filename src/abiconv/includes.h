#include <sys/attr.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <string.h>
#include <sys/stat.h>
#include <grp.h>
#include <signal.h>

typedef unsigned int in_addr_t;
struct in_addr {
   in_addr_t s_addr;
};

int inet_aton(const char *cp, struct in_addr *pin);


//// TESTING ////
struct coords {
   int x;
   int y;
};

void print_coords(const struct coords *coords);
