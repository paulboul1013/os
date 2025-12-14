gdt_start:    ; Do not remove these labels — required to compute sizes and addresses

    ; Null descriptor (mandatory first GDT entry, 8 bytes)
    dd 0x00000000         ; First 4 bytes
    dd 0x00000000         ; Next 4 bytes


; ------------------------------------------------------------------------------------
; Code Segment Descriptor
; Base  = 0x00000000
; Limit = 0x000FFFFF (with 4KB granularity)
; Flags follow Intel SDM descriptor format
; ------------------------------------------------------------------------------------
gdt_code:
    dw 0xFFFF             ; Segment limit 0–15
    dw 0x0000             ; Base address 0–15
    db 0x00               ; Base address 16–23
    db 10011010b          ; Access byte:
                          ;   Type = 0xA (Execute/Read Code Segment)
                          ;   S = 1 (Descriptor type = code/data)
                          ;   DPL = 0 (Privilege level 0)
                          ;   P = 1 (Segment present)
    db 11001111b          ; Flags and limit:
                          ;   G = 1 (4KB granularity)
                          ;   D = 1 (32-bit segment)
                          ;   L = 0 (Not 64-bit code)
                          ;   AVL = 0 (Available for software)
                          ;   Limit 19–16 = 0xF
    db 0x00               ; Base address 24–31


; ------------------------------------------------------------------------------------
; Data Segment Descriptor
; Same base and limit as code segment
; Type differs (Read/Write data segment)
; ------------------------------------------------------------------------------------
gdt_data:
    dw 0xFFFF             ; Segment limit 0–15
    dw 0x0000             ; Base address 0–15
    db 0x00               ; Base address 16–23
    db 10010010b          ; Access byte:
                          ;   Type = 0x2 (Read/Write Data Segment)
                          ;   S = 1 (Code/Data descriptor)
                          ;   DPL = 0
                          ;   P = 1
    db 11001111b          ; Same flags and limit as code segment
    db 0x00               ; Base address 24–31


gdt_end:


; ------------------------------------------------------------------------------------
; GDT Descriptor
; ------------------------------------------------------------------------------------
gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; GDT size (limit field = size − 1)
    dd gdt_start                 ; Linear base address of the GDT


; ------------------------------------------------------------------------------------
; Offsets (selectors) for use in protected mode
; These are byte offsets from gdt_start to each descriptor
; ------------------------------------------------------------------------------------
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
