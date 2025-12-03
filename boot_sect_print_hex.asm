;recevie the data in 'dx'

print_hex:
    pusha

    mov cx,0; index variable

; Strategy: get the last char of 'dx', then convert to ASCII
; Numeric ASCII values: '0' (ASCII 0x30) to '9' (0x39), so just add 0x30 to byte N.
; For alphabetic characters A-F: 'A' (ASCII 0x41) to 'F' (0x46) we'll add 0x40

hex_loop:
    cmp cx,4; loop 4 times
    je end

    ;convert last char of 'dx' to ascii
    mov ax,dx ; use 'ax' as working register
    and ax,0x00f ; 0x1234 -> 0x0004 
    add al,0x30 ; add 0x30 to N to convert it to ASCII "N"
    cmp al,0x39 ; if > 9 , add extra 8 to represent 'A' to 'F'
    jle step2
    add al,7 ; 'A' is ascii 65 instead of 58 , 65-58=7

step2:
    ;get the correct pos of the string to place ascii char 
    ;bx <- base address + string len  - index of char
    mov  bx ,HEX_OUT+5 ; base+length
    sub bx,cx 
    mov [bx],al ; copy ascii char on 'al' to the pos pointed by 'bx'
    ror dx ,4


    add cx,1
    jmp hex_loop

end:
    mov bx,HEX_OUT
    call print

    popa
    ret

HEX_OUT:
    db '0x0000' ,0 ;reserve memory for our new string