mov ah,0x0e; tty mode

mov al,'H'
int 0x10
mov al,'e'
int 0x10
mov al,'l'
int 0x10
int 0x10 ;連續兩個l，直接顯示
mov al,'o'
int 0x10


jmp $; 跳轉到現在位置=無限loop

;填充0和magic number
times 510-($-$$) db 0
dw 0xaa55