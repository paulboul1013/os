mov ah,0x0e ; open tty mode

;first way
;fail because it print the memory address
;not actual data
mov al,"1"
int 0x10
mov al, the_secret
int 0x10


;second way
;try to print cotent of the  memory address of 'the_secret'
;so the is correct way
;but，bios places  bootsector not at  address 0x7c00
mov al,"2"
int 0x10
mov al,[the_secret]
int 0x10

;third way
;Add bios staring offset 0x7c00 to the memory address of the X
;and deference the content of the pointer
;need to save offset address to the bx ，because mov al,[ax] is illegal
mov al,"3"
int 0x10
mov bx,the_secret
add bx,0x7c00
mov al,[bx]
int 0x10

;fourth way
;try a shortcut way since know that X is stored at byte 0x2d 
;but very ineffective,don't recommand this way
mov al,"4"
int 0x10
mov al,[0x7c2d]
int 0x10


jmp $

the_secret:
    db "X" ;為甚麼x的地址位移量是0x2d呢?用xxd工具來檢查


times 510-($-$$) db 0
dw 0xaa55