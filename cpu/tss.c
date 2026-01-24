#include "tss.h"
#include "gdt.h"
#include "../libc/mem.h"

// TSS entry
tss_entry_t tss_entry;

void tss_init(uint32_t kernel_ss, uint32_t kernel_esp) {
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry_t) - 1;

    // Clear TSS
    memory_set((uint8_t*)&tss_entry, 0, sizeof(tss_entry_t));

    // Set kernel stack segment and pointer (for privilege level switches)
    tss_entry.ss0 = kernel_ss;
    tss_entry.esp0 = kernel_esp;

    // Set kernel segments
    tss_entry.cs = 0x08;  // Kernel code segment
    tss_entry.ss = kernel_ss;
    tss_entry.ds = kernel_ss;
    tss_entry.es = kernel_ss;
    tss_entry.fs = kernel_ss;
    tss_entry.gs = kernel_ss;

    // Set IO map base to limit (no IO permissions bitmap)
    tss_entry.iomap_base = sizeof(tss_entry_t);

    // Install TSS descriptor in GDT entry 3 (selector 0x18)
    // Access: Present=1, DPL=0, Type=0x9 (Available 32-bit TSS)
    // = 1000 1001 = 0x89
    // Granularity: G=0, all other flags 0 = 0x00
    gdt_set_gate(3, base, limit, 0x89, 0x00);
}

void tss_set_kernel_stack(uint32_t esp) {
    tss_entry.esp0 = esp;
}
