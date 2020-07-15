   bits 64
   global _main
   extern _exit
   segment .text
_main:
   push rbp
   mov edi, 1
   call _exit
