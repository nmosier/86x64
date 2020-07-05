#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>

void *__mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) {
   write(1, "mmap\n", 5);
   return mmap((void *) 0x0f0000000, len, prot, flags | MAP_FIXED, fd, offset);
}

typedef struct { const void* replacement; const void* replacee; } interpose_t;

__attribute__((used)) static const interpose_t __interposers[]
__attribute__((section("__DATA, __interpose"))) =
   {{(void *) __mmap, (void *) mmap},
   };
