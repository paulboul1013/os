[org 0x7c00]
mov ah,0x0e ; open tty mode

;first way
;will fail again because still addressing the pointer
;and no data in the pointer to

mov al,"1"
int 0x10
mov al, the_secret
int 0x10


;second way
;solved the memory offset problem with 'org'，this is correct show
mov al,"2"
int 0x10
mov al,[the_secret]
int 0x10

;third way
;after add org，will add 0x7c00 twice,so it won't work thie time 

mov al,"3"
int 0x10
mov bx,the_secret
add bx,0x7c00
mov al,[bx]
int 0x10

;fourth way
;still work bacause you access the fixed memory address
mov al,"4"
int 0x10
mov al,[0x7c2d]
int 0x10


jmp $

the_secret:
    db "X" 


times 510-($-$$) db 0
dw 0xaa55