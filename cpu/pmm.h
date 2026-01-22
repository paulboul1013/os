#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

// 初始化實體記憶體管理器
void pmm_init(uint32_t mem_size);

// 分配一個實體頁框
uint32_t pmm_alloc_frame();

// 釋放一個實體頁框
void pmm_free_frame(uint32_t frame_addr);

// 標記特定區域為已使用（例如核心佔用的區域）
void pmm_reserve_region(uint32_t start_addr, uint32_t size);

#endif
