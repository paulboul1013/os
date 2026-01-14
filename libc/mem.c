#include "mem.h"

void memory_copy(uint8_t *source, uint8_t *dest, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void memory_set(uint8_t *dest, uint8_t val, uint32_t len) {
    uint8_t *temp = (uint8_t *)dest;
    for (; len != 0; len--) {
        *temp++ = val;
    }
}

#define HEAP_START 0x10000
#define HEAP_MAX_ADDR 0x80000 // Limit heap to 512KB for now to stay below VGA buffer (0xB8000)

static header_t *free_list = (header_t*)HEAP_START;
static int is_init = 0;

void mem_init() {
    free_list->size = HEAP_MAX_ADDR - HEAP_START;
    free_list->is_free = 1;
    free_list->next = NULL;
    is_init = 1;
}

uint32_t kmalloc(uint32_t size, int align, uint32_t *phys_addr) {
    if (!is_init) mem_init();

    // Adjust size to include header and handle alignment
    uint32_t total_size = size + sizeof(header_t);
    header_t *curr = free_list;

    while (curr) {
        if (curr->is_free && curr->size >= total_size) {
            // Handle alignment if requested (page alignment 4KB)
            if (align) {
                uint32_t addr = (uint32_t)curr + sizeof(header_t);
                if (addr & 0xFFF) {
                    uint32_t new_addr = (addr & 0xFFFFF000) + 0x1000;
                    uint32_t padding = new_addr - addr;
                    
                    if (curr->size >= total_size + padding) {
                        // We can align this block
                        // For simplicity, we just waste the padding for now or 
                        // we could split it. But let's keep it simple first.
                        // Real implementation would split the prefix.
                    } else {
                        goto next_block;
                    }
                }
            }

            // Split block if there's enough room
            if (curr->size > total_size + sizeof(header_t) + 4) {
                header_t *next_block = (header_t*)((uint32_t)curr + total_size);
                next_block->size = curr->size - total_size;
                next_block->is_free = 1;
                next_block->next = curr->next;
                
                curr->size = total_size;
                curr->next = next_block;
            }

            curr->is_free = 0;
            uint32_t ret = (uint32_t)curr + sizeof(header_t);
            if (phys_addr) *phys_addr = ret;
            return ret;
        }

    next_block:
        curr = curr->next;
    }

    // No memory found
    return 0;
}

void kfree(void *ptr) {
    if (!ptr) return;

    header_t *header = (header_t*)((uint32_t)ptr - sizeof(header_t));
    header->is_free = 1;

    // Coalesce with next block
    if (header->next && header->next->is_free) {
        header->size += header->next->size;
        header->next = header->next->next;
    }

    // Coalesce with previous block (requires traversing list or using bi-directional links)
    // For now, let's traverse from start to find previous
    header_t *curr = free_list;
    while (curr && curr != header) {
        if (curr->is_free && curr->next == header) {
            curr->size += header->size;
            curr->next = header->next;
            break;
        }
        curr = curr->next;
    }
}