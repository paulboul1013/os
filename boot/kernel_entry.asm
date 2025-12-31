global _start
[bits 32]

_start:
    [extern kernel_main] ;define calling point,must have same name as kernel.c main function
    call kernel_main ;call the c function，the linker will know  where it is placed in memory
    jmp $
