   segment .text
   global ___sigaction
   extern _sigaction
   extern __dyld_stub_binder_flag

___sigaction:
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

   ;; ARGS
   movsxd rdi, dword [rbp + 12]         ; int sig
   
   mov esi, dword [rbp + 16]    ; const struct sigaction *act
   sub rsp, 16                          ; sizeof(struct sigaction)
   mov eax, [rsi + 0]
   mov [rsp + 0], rax           ; union __sigaction_u
   mov eax, [rsi + 4]
   mov [rsp + 8], eax           ; sigset_t sa_mask
   mov eax, [rsi + 8]
   mov [rsp + 12], eax          ; int sa_flags
   mov rsi, rsp

   sub rsp, 16      
   mov rdx, rsp                 ; struct sigaction *oact

   call _sigaction

   ;; convert back struct
   mov edi, [rbp + 20]          ; struct sigaction *oact
   or edi, edi
   jz .l2
   mov rdx, [rsp + 0]
   mov [rdi + 0], edx
   mov edx, [rsp + 8]
   mov [rdi + 4], edx
   mov edx, [rsp + 12]
   mov [rdi + 8], edx
.l2:
   add rsp, 16
   
   pop rsi
   pop rdi

   leave

   mov r11d, [rsp]
   jmp r11
