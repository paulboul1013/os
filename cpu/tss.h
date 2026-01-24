#ifndef TSS_H
#define TSS_H

#include <stdint.h>

// Task State Segment structure (x86 32-bit)
typedef struct {
    uint32_t prev_tss;   // Previous TSS link (unused in software task switching)
    uint32_t esp0;       // Stack pointer for ring 0
    uint32_t ss0;        // Stack segment for ring 0
    uint32_t esp1;       // Stack pointer for ring 1 (unused)
    uint32_t ss1;        // Stack segment for ring 1 (unused)
    uint32_t esp2;       // Stack pointer for ring 2 (unused)
    uint32_t ss2;        // Stack segment for ring 2 (unused)
    uint32_t cr3;        // Page directory base
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;        // LDT selector
    uint16_t trap;       // Trap on task switch
    uint16_t iomap_base; // I/O map base address
} __attribute__((packed)) tss_entry_t;

// Initialize TSS with kernel stack info
void tss_init(uint32_t kernel_ss, uint32_t kernel_esp);

// Update TSS esp0 (called on task switch for user mode, future use)
void tss_set_kernel_stack(uint32_t esp);

// Load TSS selector into Task Register (assembly)
extern void tss_flush(void);

#endif
