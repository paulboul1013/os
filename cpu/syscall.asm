[GLOBAL syscall_stub]
syscall_stub:
    ;save all purpose registers
    pusha ;push EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
    
    ; save segment registers
    push ds
    push es
    push fs
    push gs

    ; switch to kernel data segment
    mov ax, 0x10 ;kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax


    ; call c handler function，pass into register status pointer
    push esp ;pass registers_t * to syscall_handler
    call syscall_handler
    add esp, 4 ; clean parameter

    ; restore segment registers
    pop gs
    pop fs
    pop es
    pop ds

    popa ; restore all purpose registers
    iret ; return to ring 3(user mode)