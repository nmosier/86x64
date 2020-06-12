#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include <signal.h>

#include <xed-interface.h>

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
static bool valid_ptr(p64_t ptr);
static p64_t fix_pointer(p64_t ptr);
static void fix_instruction(void *addr, _STRUCT_X86_THREAD_STATE64 *regs);
static void init_xed(void);

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
   init_xed();
   
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

static xed_state_t dstate;

static void init_xed(void) {
   xed_tables_init();
   xed_state_init2(&dstate, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
}

#define MAXPTRS 4 /* maximum number of pointers in a single instruction */

/**
 * Extracts pointers from instruction at code pointer.
 * @param addr instruction address 
 * @param output register array
 * @return output register count
 */
static void fix_instruction(void *addr, _STRUCT_X86_THREAD_STATE64 *regs) {
   xed_decoded_inst_t xedd;
   xed_decoded_inst_zero_set_mode(&xedd, &dstate);
   xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
   
   
   xed_error_enum_t err;
   if ((err = xed_decode(&xedd, addr, UINT_MAX)) != XED_ERROR_NONE) {
      fprintf(stderr, "failed to decode instruction at address 0x%p\n", addr);
      abort();
   }

   unsigned noperands = xed_decoded_inst_noperands(&xedd);
   xed_operand_values_t *operands = xed_decoded_inst_operands(&xedd);

   /* TODO: This might be using the wrong index. */
   for (unsigned i = 0; i < noperands; ++i) {
      const xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(operands, i);
      if (basereg != XED_REG_INVALID) {
         switch (basereg) {
         case XED_REG_RAX:
            regs->__rax = fix_pointer(regs->__rax);
            break;
         case XED_REG_RBX:
            regs->__rbx = fix_pointer(regs->__rbx);
            break;
         case XED_REG_RCX:
            regs->__rcx = fix_pointer(regs->__rcx);
            break;
         case XED_REG_RDX:
            regs->__rdx = fix_pointer(regs->__rdx);
            break;
         case XED_REG_RDI:
            regs->__rdi = fix_pointer(regs->__rdi);
            break;
         case XED_REG_RSI:
            regs->__rsi = fix_pointer(regs->__rsi);
            break;
         case XED_REG_RSP:
            regs->__rsp = fix_pointer(regs->__rsp);
            break;
         case XED_REG_RBP:
            regs->__rbp = fix_pointer(regs->__rbp);
            break;

         default:
            fprintf(stderr, "fix_instruction: unexpected register %d @ inst %p\n", basereg, addr);
            abort();
         }
      }
   }
}

static bool valid_ptr(p64_t ptr) {
   const p64_t ptr_high = (ptr >> 32) << 32;
   for (unsigned i = 0; i < N_ADDR_SPACES; ++i) {
      if (ptr_high == addr_spaces[i]) { return true; }
   }
   return false;
}

static p64_t fix_pointer(p64_t ptr) {
   p64_t newptr = ((p64_t) (p32_t) ptr) + addr_spaces[addr_spaces_idx];
   addr_spaces_idx = (addr_spaces_idx + 1) % N_ADDR_SPACES;   
   return newptr;
}

static void signal_handler(int sig, siginfo_t *info, ucontext_t *uap) {
   fprintf(stderr, "handling signal %d\n", sig);

   _STRUCT_X86_THREAD_STATE64 *regs = &uap->uc_mcontext->__ss;
   if (!valid_ptr(regs->__rip)) {
      regs->__rip = fix_pointer(regs->__rip);
   } else {
      fix_instruction((void *) regs->__rip, regs);
      
      /* TODO */
      //regs->__rax = fix_pointer(regs->__rax);
   }
}

#if TEST_MAIN
int _main(int argc, p32_t argv) {
   char *a = * (char **) (p64_t) argv;
      // int a = * (argc == -1 ? &argc : (int *) NULL);
   printf("%s\n", a);
   return 0;
}
#endif
