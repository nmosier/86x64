   segment .text
   global ___getopt
   extern _getopt

___getopt:
   push rbp
   mov rbp, rsp

   push rdi
   push rsi

   mov edi, [rbp + 12]
   mov esi, edi
   inc esi                      ; for NULL terminator
   mov r8d, esi
   shl rsi, 3
   sub rsp, rsi
   mov rsi, rsp                 ; rsi arg set

   ;; populate new argv
   mov rdx, rsi
   mov eax, [rbp + 16]
   jmp .entry
.loop:
   mov ecx, [rax]
   mov [rdx], rcx
   add rdx, 8
   add rax, 4
.entry:
   dec esi
   jnz .loop
   mov qword [rdx], 0

   mov edx, [rbp + 16]

   call _getopt
   
   pop rsi
   pop rdi
   leave
   mov r11d, [rsp]
   jmp r11
