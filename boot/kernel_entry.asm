[bits 32]
[extern main] ;define calling point,must have same name as kernel.c main function
call main;
jmp $
