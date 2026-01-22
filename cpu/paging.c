#include "paging.h"
#include "pmm.h"
#include "isr.h"
#include "../drivers/screen.h"
#include "../libc/stdio.h"
#include "../libc/mem.h"
#include "../libc/string.h"

static page_directory_t *kernel_directory = NULL;
static page_directory_t *current_directory = NULL;

extern void load_page_directory(uint32_t);
extern void enable_paging();

void page_fault_handler(registers_t *regs) {
    // 發生錯誤的地址存放在 CR2 暫存器中
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

    // 錯誤碼中包含了錯誤發生的細節
    int present   = !(regs->err_code & 0x1); // Page not present
    int rw        = regs->err_code & 0x2;           // Write operation?
    int us        = regs->err_code & 0x4;           // Processor was in user-mode?
    int reserved  = regs->err_code & 0x8;           // Overwritten CPU-reserved bits?
    int id        = regs->err_code & 0x10;          // Caused by an instruction fetch?
    kprint("PAGE FAULT ( ");

    if (present) kprint("not-present ");
    if (rw) kprint("read-only ");
    if (us) kprint("user-mode ");
    if (reserved) kprint("reserved ");
    if (id) kprint("instr-fetch ");
    (void)id; // 消除編譯器警告
    kprint(") at "); // hex_to_ascii 會自帶 0x

    char buf[32];
    hex_to_ascii(faulting_address, buf);
    kprint(buf);
    kprint("\n");
    
    asm volatile("hlt");
}



void init_paging() {
    // 分配 Page Directory
    // 注意：page_directory_t 包含 entries 和 tables 陣列，約 8KB，需要分配 2 個 frames
    kernel_directory = (page_directory_t*)pmm_alloc_frame();
    pmm_alloc_frame(); // 額外佔下一頁，防止 tables 陣列覆蓋到其他資料
    memory_set((uint8_t*)kernel_directory, 0, sizeof(page_directory_t));
    
    // 初始化 Page Tables，映射前 8MB (2 個 Page Tables)
    // 這樣可以涵蓋稍後可能擴展的 stack 或核心區
    for (uint32_t j = 0; j < 2; j++) {
        uint32_t pt_phys = pmm_alloc_frame();
        page_table_t *pt = (page_table_t*)pt_phys;
        memory_set((uint8_t*)pt, 0, sizeof(page_table_t));

        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t frame_idx = (j * 1024) + i;
            pt->pages[i].frame_addr = frame_idx;
            pt->pages[i].present = 1;
            pt->pages[i].rw = 1;
            pt->pages[i].user = 0;
        }
        // 放入 PD 的對應項
        kernel_directory->entries[j] = pt_phys | 0x3;
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
    cr0 |= 0x80000000; // 開啟 PG bit
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
        uint32_t pt_phys = pmm_alloc_frame();
        dir->entries[pd_idx] = pt_phys | 0x7; // Present | R/W | User
        page_table_t *table = (page_table_t*)pt_phys;
        memory_set((uint8_t*)table, 0, sizeof(page_table_t));
        return &table->pages[page_idx % 1024];
    }

    return NULL;
}
