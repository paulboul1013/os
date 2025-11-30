;infinite loop (e9 fd ff)

loop:
    jmp loop

;fill with 210 zero minus the size of the pervious code
times 510-($-$$) db 0

; magic number
dw 0xaa55
