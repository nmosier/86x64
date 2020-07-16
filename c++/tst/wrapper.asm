   segment .text
   global _main_wrapper
   extern _mmap
   extern _perror
   extern _abort
   extern _memcpy
   extern _printf
   extern _main

%define STACKSIZE 0x1000000
%define PROT_READ 0x1
%define PROT_WRITE 0x2
%define MAP_ANON 0x1000
%define MAP_PRIVATE 0x2
%define PAGEMASK 0xfff
%define PAGESIZE 0x1000
%define MAP_FAILED -1
   
_main_wrapper: 
   push rbp
   mov rbp, rsp
   sub rsp, 32
   mov [rsp + 16], rdi          ; argc
   mov [rsp + 24], rsi          ; argv
   
   xor edi, edi
   mov rsi, STACKSIZE
   mov rdx, PROT_READ | PROT_WRITE
   mov rcx, MAP_ANON | MAP_PRIVATE
   mov r8, -1
   mov r9, 0
   call _mmap
   cmp rax, MAP_FAILED
   je mmap_error
   
   mov [rsp + 0], rax           ; save new stack pointer
   mov rdi, rax
   add rdi, STACKSIZE - PAGESIZE
   mov rsi, rsp
   and rsi, ~PAGEMASK
   mov rax, rdi
   sub rax, rsi
   mov [rsp + 8], rax           ; stack pointer difference
   mov rdx, PAGESIZE
   call _memcpy

   ;; point rsp to new stack
   mov rax, [rsp + 0]
   add rax, STACKSIZE - PAGESIZE
   and rsp, PAGEMASK
   add rsp, rax

   ;; adjust argv
   mov rax, [rsp + 8]
   mov rdx, [rsp + 24]          ; argv
   add rdx, rax                 ; new argv
   mov [rsp + 24], rdx
.argv_loop: 
   add qword [rdx], rax
   add rdx, 8
   cmp qword [rdx], 0
   jne .argv_loop

   ;; success
   lea rdi, [rel success_msg]
   mov rsi, rsp
   ;; call _printf

   ;; work
   mov rdi, [rsp + 16]          ; argc
   mov rsi, [rsp + 24]          ; argv

   sub esp, 20                  ; sizeof(argc) + sizeof(argv) + sizeof(retaddr)
   and esp, ~0xf
   add esp, 8
   lea eax, [rel .ret]
   mov [rsp], eax
   mov [rsp + 4], edi
   mov [rsp + 8], esi
   
   jmp _main

.ret:
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
