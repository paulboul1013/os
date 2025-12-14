; 從磁碟 dl 讀取 dh 個扇區到 ES:BX

disk_load:
    pusha

    push dx ; 保存 dx（dh = 要讀取的扇區數，dl = 磁碟代號）

    mov ah,0x02 ; INT 0x13 讀取功能
    mov al,dh ; 要讀取的扇區數
    mov cl,0x02 ; 扇區號（0x02 是第一個可用扇區）

    mov ch,0x00 ; 磁柱號

    mov dh,0x00 ; 磁頭號

    int 0x13 ; BIOS 中斷
    jc disk_error ; 檢查錯誤（carry bit）

    pop dx ; 恢復 dx
    cmp al,dh ; 比較實際讀取的扇區數與期望值
    jne sectors_error
    popa
    ret

disk_error:
    mov bx,DISK_ERROR
    call print
    call print_nl
    mov dh,ah ; ah = 錯誤代碼
    call print_hex
    jmp disk_loop

sectors_error:
    mov bx,SECTORS_ERROR
    call print


disk_loop:
    jmp $

    
DISK_ERROR: db "Disk read error" ,0
SECTORS_ERROR: db "Incorrect number of sectors read",0