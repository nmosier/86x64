#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>

typedef uint32_t p32_t;

static void signal_handler(int sig, siginfo_t *info, ucontext_t *uap);

int _main(int argc, p32_t argv);

int main(int argc, char *argv[]) {
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
   
   return _main(argc, (p32_t) argv);
}


static void signal_handler(int sig, siginfo_t *info, ucontext_t *uap) {
   
}
