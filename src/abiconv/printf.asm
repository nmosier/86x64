   segment .text
   global ___printf
   extern _printf_conversion_f
   extern _printf

%define ARGS64_COUNT 16
%define  ARGS64_SIZE 128
   
___printf:
   push rbp
   mov rbp, rsp
   
   push rdi
   push rsi
   
   ;; sizeof(rdi, rsi, rdx, rcx, r8, r9) == 8 * 6 = 42, 16-byte aligned

   and rsp, ~0xf
   sub rsp, ARGS64_COUNT
   mov rdx, rsp                 ; reg_width_t argtypes[]
   sub rsp, ARGS64_SIZE
   mov rsi, rsp                 ; void *args64
   lea rdi, [rbp + 16]          ; const void *args32

   call _printf_conversion_f
   ;; eax -- arg count
   pop rdi
   pop rsi
   pop rdx
   pop rcx
   pop r8
   pop r9

   xor eax, eax

   call _printf

   pop rsi
   pop rdi

   leave
   ret
