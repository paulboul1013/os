#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// GDT entry structure
typedef struct {
    uint16_t limit_low;    // Limit bits 0-15
    uint16_t base_low;     // Base bits 0-15
    uint8_t  base_middle;  // Base bits 16-23
    uint8_t  access;       // Access byte
    uint8_t  granularity;  // Flags + Limit bits 16-19
    uint8_t  base_high;    // Base bits 24-31
} __attribute__((packed)) gdt_entry_t;

// GDT pointer structure for lgdt instruction
typedef struct {
    uint16_t limit;        // Size of GDT - 1
    uint32_t base;         // Linear address of GDT
} __attribute__((packed)) gdt_ptr_t;

// Initialize kernel GDT (replaces boot GDT)
void gdt_init(void);

// Set a GDT entry (used by TSS init)
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

// Load GDT and reload segment registers (assembly)
extern void gdt_flush(uint32_t gdt_ptr);

#endif
