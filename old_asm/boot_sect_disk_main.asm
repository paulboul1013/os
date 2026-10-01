[org 0x7c00]

mov bp,0x8000 ; 設定堆疊位置
mov sp,bp

mov bx,0x9000 ; 緩衝區位置 ES:BX = 0x9000
mov dh,2 ; 讀取 2 個扇區

; BIOS 會設定 dl 為啟動磁碟代號
; 如有問題，可使用 -fda 標誌：qemu -fda file.bin

call disk_load

mov dx,[0x9000] ; 讀取第一個載入的字組（0xdada）
call print_hex

call print_nl

mov dx, [0x9000+512] ; 第二個扇區的第一個字組（0xface）
call print_hex

jmp $

%include "boot_sect_print.asm"
%include "boot_sect_print_hex.asm"
%include "boot_sect_disk.asm"

times 510-($-$$) db 0
dw 0xaa55

; 啟動扇區 = 磁柱 0、磁頭 0、扇區 1
; 以下為扇區 2...
times 256 dw 0xdada ; 扇區 2 = 512 bytes
times 256 dw 0xface ; 扇區 3 = 512 bytes


