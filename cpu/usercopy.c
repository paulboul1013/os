#include "usercopy.h"
#include "paging.h"
#include "../libc/mem.h"

/* Single CPU, shared page directory. Call with IRQs disabled; mappings must
 * remain stable until copying completes. Zero-length buffers are not touched. */
int user_range_valid(const void *ptr, uint32_t len, int write) {
    uint32_t start = (uint32_t)ptr;
    if (!len) return 1;
    if (!start || len - 1 > UINT32_MAX - start) return 0;
    uint32_t end = (start + len - 1) & 0xfffff000;
    for (uint32_t page = start & 0xfffff000;; page += 4096) {
        if (!paging_user_access(page, write)) return 0;
        if (page == end) return 1;
    }
}
int copy_from_user(void *dst, const void *src, uint32_t len) {
    if (!user_range_valid(src, len, 0)) return USER_EFAULT;
    memory_copy((uint8_t*)src, dst, len);
    return 0;
}
int copy_to_user(void *dst, const void *src, uint32_t len) {
    if (!user_range_valid(dst, len, 1)) return USER_EFAULT;
    memory_copy((uint8_t*)src, dst, len);
    return 0;
}
int copy_string_from_user(char *dst, const char *src, uint32_t capacity) {
    uint32_t address = (uint32_t)src;
    for (uint32_t i = 0; i < capacity; i++) {
        if (i > UINT32_MAX - address ||
            !user_range_valid((void*)(address + i), 1, 0)) return USER_EFAULT;
        dst[i] = *(const char*)(address + i);
        if (!dst[i]) return 0;
    }
    return USER_ENAMETOOLONG;
}
