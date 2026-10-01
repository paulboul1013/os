[EXTERN syscall_handler]
[GLOBAL syscall_stub]
syscall_stub:
    ; 為了讓堆疊格式對齊 registers_t，我們需要推入假錯誤碼和中斷號
    push dword 0     ; err_code
    push dword 0x80  ; int_no

    ; save all purpose registers
    pusha           ; push edi,esi,ebp,esp,ebx,edx,ecx,eax
    
    ; DS starts registers_t; ES/FS/GS are private saves below that frame.
    xor eax, eax
    mov ax, ds
    push eax
    mov ax, es
    push eax
    mov ax, fs
    push eax
    mov ax, gs
    push eax

    ; switch to kernel data segment
    mov ax, 0x10    ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; call c handler function，pass into register status pointer
    lea eax, [esp + 12]
    push eax        ; pass pointer to registers_t.ds
    cld
    call syscall_handler
    add esp, 4      ; clean parameter

    ; restore segment registers
    pop eax
    mov gs, ax
    pop eax
    mov fs, ax
    pop eax
    mov es, ax
    pop eax
    mov ds, ax

    popa            ; restore all purpose registers
    add esp, 8      ; 清除推入的 err_code 以及 int_no
    iret            ; return to ring 3(user mode)
