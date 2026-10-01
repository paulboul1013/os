[org 0x7c00] ;tell assembler that our offset is bootsector code


mov bx,HELLO
call print

call print_nl

mov bx,GOODBYE
call print

call print_nl

mov dx,0x12fe
call print_hex
call print_nl

mov dx,0xbad
call print_hex
call print_nl

mov dx,0x003f
call print_hex
call print_nl

jmp $

%include "boot_sect_print.asm"
%include "boot_sect_print_hex.asm"

;data
HELLO:
    db 'Hello,World' , 0

GOODBYE:
    db 'Goodbye' ,0 

times 510-($-$$) db 0
dw 0xaa55