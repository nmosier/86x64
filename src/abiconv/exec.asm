   segment .text
   global ___%+SYMBOL
   extern _%+SYMBOL
   extern __dyld_stub_binder_flag

___%+SYMBOL:
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

   mov edi, [rbp + 16]          ; char *argv[]
   mov edx, edi
   ;; get length
   xor eax, eax
.count:
   inc eax
   sub rsp, 8
   cmp dword [edi], 0
   lea edi, [edi + 4]
   jne .count

   mov rsi, rsp

.argv:
   dec eax
   mov ecx, [rdx + rax * 4]
   mov [rsi + rax * 8], rcx
   jnz .argv

   mov edi, [rbp + 12]          ; const char *file

   and rsp, ~0xf
   call _%+SYMBOL

   lea rsp, [rbp - 16]
   pop rsi
   pop rdi
   leave
   mov r11d, [rsp]
   jmp r11
