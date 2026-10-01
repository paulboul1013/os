#include "paging.h"
#include "pmm.h"
#include "isr.h"
#include "../drivers/screen.h"
#include "../libc/stdio.h"
#include "../libc/mem.h"
#include "../libc/string.h"

static page_directory_t *kernel_directory = NULL;
static page_directory_t *current_directory = NULL;

static uint32_t paging_frame(void) {
    uint32_t frame = pmm_alloc_frame();
    if (!frame || frame >= 0x800000) {
        kprint("KERNEL PANIC: page table allocation failed\n");
        for (;;) asm volatile("cli; hlt");
    }
    return frame;
}

void page_fault_handler(registers_t *regs) {
    exception_fault(regs);
}

void init_paging() {
    // 分配 Page Directory
    kernel_directory = (page_directory_t*)paging_frame();
    memory_set((uint8_t*)kernel_directory, 0, sizeof(page_directory_t));
    
    // 初始化 Page Tables，映射前 8MB (2 個 Page Tables)
    // 這樣可以涵蓋稍後可能擴展的 stack 或核心區
    for (uint32_t j = 0; j < 2; j++) {
        uint32_t pt_phys = paging_frame();
        page_table_t *pt = (page_table_t*)pt_phys;
        memory_set((uint8_t*)pt, 0, sizeof(page_table_t));

        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t frame_idx = (j * 1024) + i;
            pt->pages[i].frame_addr = frame_idx;
            pt->pages[i].present = 1;
            pt->pages[i].rw = 1;
            pt->pages[i].user = 0; // Supervisor by default
        }
        // 放入 PD 的對應項
        kernel_directory->entries[j] = pt_phys | 0x7; // Present | R/W | User
    }
    
    extern char __user_text_start, __user_rodata_end;
    extern char __user_data_start, __user_data_end;
    for (uint32_t a = (uint32_t)&__user_text_start; a < (uint32_t)&__user_rodata_end; a += PAGE_SIZE) {
        page_t *p = get_page(a, 0, kernel_directory);
        p->user = 1; p->rw = 0;
    }
    for (uint32_t a = (uint32_t)&__user_data_start; a < (uint32_t)&__user_data_end; a += PAGE_SIZE) {
        get_page(a, 0, kernel_directory)->user = 1;
    }

    // 自我引用 (Recursive Mapping)
    kernel_directory->entries[PAGE_RECURSIVE_SLOT] = (uint32_t)kernel_directory | 0x3;

    
    // 註冊 Page Fault 中斷處理器 (INT 14)
    register_interrupt_handler(14, page_fault_handler);
    
    // 切換並開啟分頁
    current_directory = kernel_directory;
    switch_page_directory(kernel_directory);
}

void switch_page_directory(page_directory_t* dir) {
    current_directory = dir;
    // 傳入的是實體地址
    asm volatile("mov %0, %%cr3" :: "r"((uint32_t)dir));
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80010000; // PG + WP: supervisor writes also honor read-only PTEs
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

page_t* get_page(uint32_t address, int make, page_directory_t* dir) {
    uint32_t page_idx = address / PAGE_SIZE;
    uint32_t pd_idx = page_idx / 1024;
    
    if (dir->entries[pd_idx] & 0x1) {
        // PT 已存在
        // 如果開啟了分頁，我們通常需要透過映射來存取。
        // 這裡暫時假設我們仍在初始化階段或透過虛擬地址指標存取
        page_table_t *table = (page_table_t*)(dir->entries[pd_idx] & 0xFFFFF000);
        return &table->pages[page_idx % 1024];
    } else if (make) {
        uint32_t pt_phys = paging_frame();
        dir->entries[pd_idx] = pt_phys | 0x7; // Present | R/W | User
        page_table_t *table = (page_table_t*)pt_phys;
        memory_set((uint8_t*)table, 0, sizeof(page_table_t));
        return &table->pages[page_idx % 1024];
    }

    return NULL;
}

int paging_user_access(uint32_t address, int write) {
    uint32_t pde = current_directory->entries[address >> 22];
    if ((pde & 5) != 5 || (pde & 0x80) || (write && !(pde & 2))) return 0;
    page_t *p = get_page(address, 0, current_directory);
    return p && p->present && p->user && (!write || p->rw);
}
void paging_set_user_stack(uint32_t address, int enabled) {
    page_t *p = get_page(address, 0, current_directory);
    p->user = enabled != 0;
    p->rw = 1;
    asm volatile("invlpg (%0)" :: "r"(address) : "memory");
}
