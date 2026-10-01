mov ah,0x0e ; tty

mov al,[the_secret]
int 0x10; doesn't work

mov bx ,0x7c0; the segment is automatically <<4 ，so don't need set 0x7c00
mov ds,bx

;note: from now on all memory references will be offset by ds implicitly
mov al,[the_secret]
int 0x10


mov al,[es:the_secret]
int 0x10; doesn't look right ，but es isn't currently 0x000?

mov bx,0x7c0
mov es,bx
mov al,[es:the_secret]
int 0x10


jmp $

the_secret:
    db "X"

times 510-($-$$) db 0
dw 0xaa55