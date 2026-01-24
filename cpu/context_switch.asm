[bits 32]
section .text

; ============================================================
; context_switch(uint32_t *old_esp, uint32_t new_esp)
;
; Saves current task's context and switches to new task.
;
; Parameters (cdecl calling convention):
;   [esp+4]  = pointer to old task's esp storage location
;   [esp+8]  = new task's saved esp value
;
; Stack layout after pushes (before switch):
;   [esp+0]  = EFLAGS
;   [esp+4]  = EDI
;   [esp+8]  = ESI
;   [esp+12] = EBX
;   [esp+16] = EBP
;   [esp+20] = return address
;   [esp+24] = old_esp pointer (first argument)
;   [esp+28] = new_esp (second argument)
; ============================================================

global context_switch
context_switch:
    ; === SAVE CURRENT TASK'S CONTEXT ===

    ; Save callee-saved registers (cdecl convention)
    push ebp
    push ebx
    push esi
    push edi
    pushf                       ; Save EFLAGS (includes interrupt flag)

    ; Get pointer to old_esp and save current stack pointer
    ; After 5 pushes (5*4=20 bytes) + return address (4 bytes) = 24 bytes
    mov eax, [esp + 24]         ; eax = old_esp pointer
    mov [eax], esp              ; *old_esp = current ESP

    ; === SWITCH TO NEW TASK ===

    ; Load new stack pointer
    ; Note: We read new_esp from OLD stack before switching
    mov eax, [esp + 28]         ; eax = new_esp
    mov esp, eax                ; ESP = new_esp

    ; === RESTORE NEW TASK'S CONTEXT ===

    popf                        ; Restore EFLAGS
    pop edi
    pop esi
    pop ebx
    pop ebp

    ret                         ; Return to new task (pops EIP from new stack)


; ============================================================
; tss_flush - Load TSS selector into Task Register
;
; Must be called after setting up TSS in GDT.
; TSS selector = 0x18 (GDT entry 3: 3 * 8 = 24 = 0x18)
; ============================================================

global tss_flush
tss_flush:
    mov ax, 0x18                ; TSS selector (GDT entry 3)
    ltr ax                      ; Load Task Register
    ret


; ============================================================
; gdt_flush - Load new GDT and reload segment registers
;
; Parameter:
;   [esp+4] = pointer to gdt_ptr structure
; ============================================================

global gdt_flush
gdt_flush:
    mov eax, [esp + 4]          ; Get pointer to gdt_ptr
    lgdt [eax]                  ; Load the GDT

    ; Reload segment registers with new selectors
    mov ax, 0x10                ; Data segment selector (entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with code segment selector
    jmp 0x08:.flush_done        ; Code segment selector (entry 1)
.flush_done:
    ret
