gdt_start: ;don't remove the labels ,all needed to compute sizes and jumps 
    ; the GDT starts with a null 8 byte
    dd 0x0 ;4 byte
    dd 0x0


; GDT for code segment. base = 0x00000000, length = 0xfffff
; for flags, refer to os-dev.pdf document, page 36
gdt_code:
    dw 0xffff    ; segment limit, bits 0-15
    dw 0x0       ; segment base, bits 0-15
    db 0x0       ; segment base, bits 16-23
    db 10011010b ; flags (8 bits) Type=0xA ,S=1,DPL=0,P=1
    db 11001111b ; flags (4 bits) + segment length, bits 16-19， G=1, D=1, L=0, AVL=0, Limit19:16=0xF
    db 0x0       ; segment base, bits 24-31

; GDT for data segment. base and length identical to code segment
; some flags changed, again, refer to os-dev.pdf
gdt_data:
    dw 0xffff ;limit
    dw 0x0 ;base
    db 0x0
    db 10010010b ;Type=0x2
    db 11001111b
    db 0x0


gdt_end:
    

;GDT descriptor
gdt_descriptor:
    dw gdt_end-gdt_start -1 ; size (16 bit), always one less of its true size
    dd gdt_start ; address(32 bit)


;define some constants for later use
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start