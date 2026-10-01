#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "isr.h"

// Page Table Entry (PTE) 結構

typedef struct {
    uint32_t present    : 1;   // 頁面是否在記憶體中
    uint32_t rw         : 1;   // 0: 唯讀, 1: 讀寫
    uint32_t user       : 1;   // 0: 核心權限, 1: 用戶權限
    uint32_t write_through : 1;
    uint32_t cache_disable : 1;
    uint32_t accessed : 1;
    uint32_t dirty : 1;
    uint32_t pat : 1;
    uint32_t global : 1;
    uint32_t unused : 3;
    uint32_t frame_addr : 20;  // 實體頁框地址 (高 20 位)
} page_t;

// Page Table (PT)
typedef struct {
    page_t pages[1024];
} page_table_t;

// Page Directory (PD)
typedef struct {
    // 實體地址 entry (用於 CPU)
    uint32_t entries[1024]; 
} page_directory_t;

// 自我引用 (Recursive Mapping) 常量
// 假設我們使用第 1023 個 PDE
#define PAGE_RECURSIVE_SLOT 1023
#define PAGE_METADATA_BASE 0xFFC00000
#define PAGE_DIRECTORY_BASE (PAGE_METADATA_BASE + (PAGE_RECURSIVE_SLOT * 0x1000))

int paging_user_access(uint32_t address, int write);
void paging_set_user_stack(uint32_t address, int enabled);

// 初始化分頁系統
void init_paging();

// 切換分頁目錄
void switch_page_directory(page_directory_t* dir);

// 獲取特定虛擬地址的 PTE，如果 make 為真且不存在則建立
page_t* get_page(uint32_t address, int make, page_directory_t* dir);

// 分頁錯誤處理程序
void page_fault_handler(registers_t *regs);


#endif
