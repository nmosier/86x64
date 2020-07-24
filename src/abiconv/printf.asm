   segment .text
   global ___printf
   extern _printf_conversion_f
   extern _printf
   extern __dyld_stub_binder_flag

%define ARGS64_COUNT 16
%define  ARGS64_SIZE 128
   
___printf:
   cmp qword [rel __dyld_stub_binder_flag], 0
   je .l1
   mov rsp, qword [rel __dyld_stub_binder_flag]
   add rsp, 16
   mov qword [rel __dyld_stub_binder_flag], 0
.l1:
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
   lea rdi, [rbp + 12]          ; const void *args32

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

   lea rsp, [rbp - 16]
   pop rsi
   pop rdi

   leave

   mov r11d, dword [rsp]
   add rsp, 4
   jmp r11
