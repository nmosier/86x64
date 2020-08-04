   segment .text
   global __dyld_stub_binder
   extern dyld_stub_binder
   global __dyld_stub_binder_flag

%define VERBOSE 0

%if VERBOSE
   extern _printf
%endif
   
__dyld_stub_binder:
   mov r11, rsp
   mov [rel __dyld_stub_binder_flag], rsp
   
   and rsp, ~0xf
%if VERBOSE
   push rdi
   push rsi
   push rdx
   push rcx
   push r8
   push r9
   push r10
   push r11
   mov esi, [r11 + 8]
   lea rdi, [rel fmtstr]
   call _printf
   pop r11
   pop r10
   pop r9
   pop r8
   pop rcx
   pop rdx
   pop rsi
   pop rdi
%endif
   sub rsp, 8
   push qword [r11 + 8]
   push qword [r11 + 0]
   jmp dyld_stub_binder
   
   segment .data
__dyld_stub_binder_flag: dq 0

fmtstr:  db "%x", 0x0a, 0x0
