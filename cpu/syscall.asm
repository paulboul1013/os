[EXTERN syscall_handler]
[GLOBAL syscall_stub]
syscall_stub:
    ; 為了讓堆疊格式對齊 registers_t，我們需要推入假錯誤碼和中斷號
    push dword 0     ; err_code
    push dword 0x80  ; int_no

    ; save all purpose registers
    pusha           ; push edi,esi,ebp,esp,ebx,edx,ecx,eax
    
    ; registers_t 預期在這裡只有推入一個 32 bits 的 ds
    mov ax, ds
    push eax        ; 存下原本的使用者 ds (4 bytes)

    ; switch to kernel data segment
    mov ax, 0x10    ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; call c handler function，pass into register status pointer
    push esp        ; pass registers_t * to syscall_handler
    cld
    call syscall_handler
    add esp, 4      ; clean parameter

    ; restore segment registers
    pop eax         ; 彈出原本的 ds
    mov ds, ax      
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; restore all purpose registers
    add esp, 8      ; 清除推入的 err_code 以及 int_no
    iret            ; return to ring 3(user mode)