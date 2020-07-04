   segment .text
   global _main
   extern _mmap
   extern _perror
   extern _abort
   extern _memcpy
   extern _printf

%define STACKSIZE 0x1000000
%define PROT_READ 0x1
%define PROT_WRITE 0x2
%define MAP_ANON 0x1000
%define MAP_PRIVATE 0x2
%define PAGEMASK 0xfff
%define PAGESIZE 0x1000
%define MAP_FAILED -1
   
_main:
   push rbp
   mov rbp, rsp
   sub rsp, 16
   xor edi, edi
   mov rsi, STACKSIZE
   mov rdx, PROT_READ | PROT_WRITE
   mov rcx, MAP_ANON | MAP_PRIVATE
   mov r8, -1
   mov r9, 0
   call _mmap
   cmp rax, MAP_FAILED
   je mmap_error
   
   mov [rsp + 8], rax           ; save new stack pointer
   mov rdi, rax
   add rdi, STACKSIZE - PAGESIZE
   mov rsi, rsp
   and rsi, ~PAGEMASK
   mov rdx, PAGESIZE
   call _memcpy

   mov rax, [rsp + 8]
   add rax, STACKSIZE - PAGESIZE
   and rsp, PAGEMASK
   add rsp, rax

   lea rdi, [rel success_msg]
   mov rsi, rsp
   call _printf
   
   ;; todo
   
   xor eax, eax
   leave
   ret
   

mmap_error:
   lea rdi, [rel mmap_errstr]
   call _perror
   call _abort
   

   segment .data
mmap_errstr:   db "mmap",0
success_msg:   db "%p",0xa,0
