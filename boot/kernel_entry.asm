global _start
[bits 32]

_start:
    cld
    extern __user_data_file_end, __bss_end
    mov edi, __user_data_file_end
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    [extern call_global_constructors]
    call call_global_constructors
    
    [extern kernel_main] ;define calling point,must have same name as kernel.c main function
    call kernel_main ;call the c function，the linker will know  where it is placed in memory
    jmp $
