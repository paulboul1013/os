; Read DH sectors from drive DL into 0000:BX.
; Uses 1.44MB floppy CHS geometry: 18 sectors/track, 2 heads.

disk_load:
    pusha

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov [DISK_DRIVE], dl
    mov [SECTORS_LEFT], dh
    mov byte [CURRENT_SECTOR], 0x02
    mov byte [CURRENT_HEAD], 0x00
    mov byte [CURRENT_CYLINDER], 0x00

.read_loop:
    cmp byte [SECTORS_LEFT], 0
    je .done

    mov ah, 0x02
    mov al, 0x01
    mov ch, [CURRENT_CYLINDER]
    mov cl, [CURRENT_SECTOR]
    mov dh, [CURRENT_HEAD]
    mov dl, [DISK_DRIVE]

    push es
    push bx
    int 0x13
    pop bx
    pop es

    pushf
    push ax
    xor ax, ax
    mov ds, ax
    pop ax
    popf
    jc disk_error

    cmp al, 0x01
    jne sectors_error

    add bx, 512
    jnc .buffer_ready
    mov ax, es
    add ax, 0x1000
    mov es, ax
.buffer_ready:
    dec byte [SECTORS_LEFT]
    inc byte [CURRENT_SECTOR]
    cmp byte [CURRENT_SECTOR], 19
    jb .read_loop

    mov byte [CURRENT_SECTOR], 1
    inc byte [CURRENT_HEAD]
    cmp byte [CURRENT_HEAD], 2
    jb .read_loop

    mov byte [CURRENT_HEAD], 0
    inc byte [CURRENT_CYLINDER]
    jmp .read_loop

.done:
    popa
    ret

disk_error:
    mov bx, DISK_ERROR
    call print
    call print_nl
    jmp disk_loop

sectors_error:
    mov bx, SECTORS_ERROR
    call print

disk_loop:
    jmp $

DISK_ERROR: db "Disk read error" ,0
SECTORS_ERROR: db "Incorrect number of sectors read",0
DISK_DRIVE: db 0
SECTORS_LEFT: db 0
CURRENT_SECTOR: db 0
CURRENT_HEAD: db 0
CURRENT_CYLINDER: db 0
