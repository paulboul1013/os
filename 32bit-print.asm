[bits 32]; using 32-bit protected mode


;constant defined
VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f ; the color byte for each character

print_string_pm:
    pusha
    mov eax, VIDEO_MEMORY


print_string_pm_loop:
    mov al,[ebx] ; [ebx] is the address of input character
    mov ah,WHITE_ON_BLACK ;setting character color


    cmp al,0 ; check if end of string
    je print_string_pm_done

    mov [edx] ,ax ;store character  + attribute in video memory
    add ebx ,1 ; next char 
    add edx,2 ;next video memory position

    jmp print_string_pm_loop

print_string_pm_done:
    popa
    ret

