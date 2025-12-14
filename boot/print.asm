print:
    pusha

    
;start: program like under code
;while (string[i]!=0) {print string[i]; i++;}


start:
    mov al,[bx]; 'bx' is the base address for the string
    cmp al,0
    je done


    mov ah,0x0e; tty mode
    int 0x10; 'al' already have character content

    add bx,1
    jmp start

done:
    popa
    ret


print_nl:
    pusha


    mov ah,0x0e
    mov al,0x0a; newline char
    int 0x10
    mov al, 0x0d;carriage return
    int 0x10

    popa
    ret