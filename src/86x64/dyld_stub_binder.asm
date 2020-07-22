   segment .text
   global __dyld_stub_binder
   extern dyld_stub_binder
   global __dyld_stub_binder_flag

__dyld_stub_binder:
   mov r11, rsp
   mov [rel __dyld_stub_binder_flag], rsp
   
   and rsp, ~0xf
   sub rsp, 8

   push qword [r11 + 8]
   push qword [r11 + 0]
   jmp dyld_stub_binder
   
   segment .data
__dyld_stub_binder_flag: dq 0
   
