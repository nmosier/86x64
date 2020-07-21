1d0d4:
   test al, al
   je rebase_type_pointer
rebase_type_abs32:   
   add dword ptr [r14], r12d
   add r14, 4
   jmp join
rebase_type_pointer: 
   add qword ptr [r14], r12
   add r14, 8
join:
   nop
   ...
   1d10a:   


0:  84 c0                   test   al,al
2:  74 09                   je     d <rebase_type_pointer>
0000000000000004 <rebase_type_abs32>:
4:  45 01 26                add    DWORD PTR [r14],r12d
7:  49 83 c6 08             add    r14,0x8
b:  eb 07                   jmp    14 <join>
000000000000000d <rebase_type_pointer>:
d:  4d 01 26                add    QWORD PTR [r14],r12
10: 49 83 c6 08             add    r14,0x8
0000000000000014 <join>:
14: 90                      nop


offset:   0x1e0d8
