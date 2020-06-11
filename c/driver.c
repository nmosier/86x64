#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>

#include "util.h"

#define TEST_MAIN 1

typedef uint32_t p32_t;
typedef uint64_t p64_t;

struct addr_range {
   p64_t begin;
   p64_t end;
};

static void signal_handler(int sig, siginfo_t *info, ucontext_t *uap);
static void init_ranges(void);

int _main(int argc, p32_t argv);

#define N_RANGES
static struct addr_range addr_ranges[N_RANGES] = {0}; 

int main(int argc, char *argv[]) {
   /* install signal handler */
   struct sigaction sa;
   sa.sa_sigaction = (void (*)(int, siginfo_t *, void *)) signal_handler;
   sigemptyset(&sa.sa_mask);
   sigaddset(&sa.sa_mask, SIGBUS);
   sigaddset(&sa.sa_mask, SIGSEGV);
   sa.sa_flags = SA_SIGINFO;

   if (sigaction(SIGBUS,  &sa, NULL) < 0 ||
       sigaction(SIGSEGV, &sa, NULL) < 0) {
      perror("sigaction");
      return 1;
   }

   init_ranges();
   
   return _main(argc, (p32_t) argv);
}

static p64_t addr_spaces[] = {0x100000000,    /* code & user dylibs */
                              0x7ffe00000000, /* stack */
                              0x7fff00000000, /* system dylibs */
                              0x0             /* heap (unknown) */
};
#define N_ADDR_SPACES ARRLEN(addr_spaces)
static unsigned int addr_spaces_idx = 0;

static void init_ranges(void) {
   void *heap = malloc(1);
   assert(heap);
   addr_spaces[3] = (((p64_t) heap) >> 32) << 32; /* clear bottom 32 bits */
   free(heap);
}

static void signal_handler(int sig, siginfo_t *info, ucontext_t *uap) {
   fprintf(stderr, "handling signal %d\n", sig);
   uap->uc_mcontext->__ss.__rax =
      ((uint64_t) ((uint32_t) uap->uc_mcontext->__ss.__rax)) + addr_spaces[addr_spaces_idx];
   addr_spaces_idx = (addr_spaces_idx + 1) % N_ADDR_SPACES;
   
   // TODO
   // uap->uc_mcontext->__ss.__rax = (uintptr_t) info;
}

#if TEST_MAIN
int _main(int argc, p32_t argv) {
   char *a = * (char **) (p64_t) argv;
      // int a = * (argc == -1 ? &argc : (int *) NULL);
   printf("%s\n", a);
   return 0;
}
#endif
