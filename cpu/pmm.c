#include "pmm.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"
#include "../libc/stdio.h"

// 支援最大 128MB 實體記憶體 (128MB / 4KB = 32768 frames)
// 32768 bits = 4096 bytes
#define MAX_FRAMES 32768
#define BITMAP_SIZE (MAX_FRAMES / 8)

static uint8_t pmm_bitmap[BITMAP_SIZE];
static uint32_t total_frames = 0;

void pmm_init(uint32_t mem_size) {
    total_frames = mem_size / PAGE_SIZE;
    if (total_frames > MAX_FRAMES) total_frames = MAX_FRAMES;

    // 初始全部標記為 0 (空閒)
    memory_set(pmm_bitmap, 0, BITMAP_SIZE);
}

void pmm_reserve_region(uint32_t start_addr, uint32_t size) {
    uint32_t start_frame = start_addr / PAGE_SIZE;
    uint32_t end_frame = (start_addr + size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t i = start_frame; i < end_frame && i < total_frames; i++) {
        pmm_bitmap[i / 8] |= (1 << (i % 8));
    }
}

uint32_t pmm_alloc_frame() {
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!(pmm_bitmap[i / 8] & (1 << (i % 8)))) {
            pmm_bitmap[i / 8] |= (1 << (i % 8));
            return i * PAGE_SIZE;
        }
    }
    return 0; // Out of memory
}

void pmm_free_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / PAGE_SIZE;
    if (frame < total_frames) {
        pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
    }
}
