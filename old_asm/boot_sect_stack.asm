mov ah , 0x0e ; tty mode

mov bp,0x8000 ;base地址遠離0x7c00 所以不怕亂寫到重要地址
mov sp,bp; 如果stack是空的，bp和sp會在同一地址

push 'A'
push 'B'
push 'C'

;顯示stack是如何由上而下長
mov al,[0x7ffe] ;0x8000-2
int 0x10


;然而，不要嘗試存取[0x8000]，因為不會有啥作用
;你只能存取stack的top，在0x7ffe
mov al,[0x8000]
int 0x10


pop bx
mov al,bl
int 0x10 ;print C


pop bx
mov al,bl
int 0x10 ;print B


pop bx
mov al,bl
int 0x10 ;print A

;資料已經都被pop出去，現在的stack是都空的
mov al,[0x8000]
int 0x10

jmp $

times 510-($-$$) db 0
dw 0xaa55