#include "gdt.h"

// GDT with 4 entries: null, code, data, TSS
#define GDT_ENTRIES 4

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // Set base address
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    // Set limit
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = (limit >> 16) & 0x0F;

    // Set flags and access
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base = (uint32_t)&gdt;

    // Entry 0: Null descriptor (required)
    gdt_set_gate(0, 0, 0, 0, 0);

    // Entry 1: Code segment (selector 0x08)
    // Base: 0, Limit: 0xFFFFF (4GB with 4K granularity)
    // Access: Present=1, DPL=0, S=1, Type=0xA (Execute/Read)
    // = 1001 1010 = 0x9A
    // Gran: G=1 (4K), D=1 (32-bit), L=0, AVL=0 = 1100 0000 = 0xC0
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);

    // Entry 2: Data segment (selector 0x10)
    // Access: Present=1, DPL=0, S=1, Type=0x2 (Read/Write)
    // = 1001 0010 = 0x92
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);

    // Entry 3: TSS (selector 0x18)
    // Will be set up by tss_init() - leave empty for now
    gdt_set_gate(3, 0, 0, 0, 0);

    // Load the new GDT
    gdt_flush((uint32_t)&gdt_ptr);
}
