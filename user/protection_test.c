#ifdef PROTECTION_TEST
#include <stdint.h>
#include "../kernel/syscall.h"
#include "../cpu/usercopy.h"

volatile uint32_t test_mode, test_address, test_completed, test_survivor, test_last_stack;
static char scratch[8192] __attribute__((aligned(4096)));
extern char __user_data_end;

static int call(uint32_t n, uint32_t a, uint32_t b, uint32_t c) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory", "cc");
    return ret;
}
#define PTR(p) ((uint32_t)(p))
static void check(int yes) {
    if (!yes) {
        call(SYS_WRITE, PTR("USER TEST FAILED\n"), 0, 0);
        call(SYS_EXIT, 0, 0, 0);
        for (;;) {}
    }
}
void user_test_survivor(void) {
    for (;;) { test_survivor++; call(SYS_YIELD, 0, 0, 0); }
}
void user_protection_test(void) {
    uint32_t esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    test_last_stack = esp & 0xfffff000;
    if (test_mode == 1) { volatile uint32_t x = *(volatile uint32_t*)test_address; (void)x; }
    if (test_mode == 2) *(volatile uint32_t*)test_address = 0xdeadbeef;
    if (test_mode == 3) asm volatile("int $0x20");
    if (test_mode == 4) asm volatile("ud2");
    if (test_mode) {
        call(SYS_WRITE, PTR("FAULT DID NOT FIRE\n"), 0, 0);
        call(SYS_EXIT, 0, 0, 0);
        for (;;) {}
    }
    const char *name = "probe";
    check(call(SYS_FS_CREATE, PTR(name), 0, 0) == 0);
    check(call(SYS_FS_WRITE, PTR(name), PTR("safe"), 4) == 0);
    check(call(SYS_FS_READ, PTR(name), PTR(scratch), 4) == 4);
    check(scratch[0] == 's' && scratch[3] == 'e');
    uint32_t bad[] = {0, 0x8000, 0x800000, 0xfffffff0, 0xfffff000};
    uint32_t names[] = {SYS_WRITE, SYS_FS_CREATE, SYS_FS_DELETE, SYS_FS_READ, SYS_FS_WRITE};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
        for (unsigned j = 0; j < sizeof(names)/sizeof(names[0]); j++)
            check(call(names[j], bad[i], PTR(scratch), 8) == USER_EFAULT);
        check(call(SYS_READ, bad[i], 32, 0) == USER_EFAULT);
        check(call(SYS_FS_READ, PTR(name), bad[i], 32) == USER_EFAULT);
        check(call(SYS_FS_WRITE, PTR(name), bad[i], 32) == USER_EFAULT);
    }
    /* Every string position: bounded but unterminated, then crossing into kernel. */
    for (unsigned i = 0; i < sizeof(scratch); i++) scratch[i] = 'x';
    char *edge = &__user_data_end - 1;
    *edge = 'x';
    for (unsigned j = 0; j < sizeof(names)/sizeof(names[0]); j++) {
        check(call(names[j], PTR(scratch), PTR(scratch), 8) == USER_ENAMETOOLONG);
        check(call(names[j], PTR(edge), PTR(scratch), 8) == USER_EFAULT);
    }
    check(call(SYS_READ, PTR(edge), 2, 0) == USER_EFAULT);
    check(call(SYS_FS_READ, PTR(name), PTR(edge), 2) == USER_EFAULT);
    check(call(SYS_FS_WRITE, PTR(name), PTR(edge), 2) == USER_EFAULT);
    check(call(SYS_READ, PTR(scratch), 0xffffffff, 0) == USER_EFAULT);
    check(call(SYS_FS_READ, PTR(name), PTR(scratch), 0xffffffff) == USER_EFAULT);
    check(call(SYS_FS_WRITE, PTR(name), PTR(scratch), 0xffffffff) == USER_EFAULT);
    check(call(SYS_READ, PTR(name), 1, 0) == USER_EFAULT);
    check(call(SYS_FS_READ, PTR(name), PTR(name), 1) == USER_EFAULT);
    /* Valid cross-page buffers remain accepted. Failed writes did not mutate FS. */
    check(call(SYS_FS_READ, PTR(name), PTR(scratch + 4094), 4) == 4);
    check(scratch[4094] == 's' && scratch[4097] == 'e');
    check(call(SYS_FS_WRITE, PTR(name), PTR(scratch + 4094), 4) == 0);
    /* Empty FS buffers are never dereferenced; oversized valid input is rejected. */
    check(call(SYS_READ, PTR(scratch), 0, 0) == -1);
    check(call(SYS_FS_READ, PTR(name), 0, 0) == 0);
    check(call(SYS_FS_WRITE, PTR(name), PTR(scratch), 4097) == -4);
    check(call(SYS_FS_READ, PTR(name), PTR(scratch), 4) == 4);
    check(scratch[0] == 's' && scratch[3] == 'e');
    check(call(SYS_FS_WRITE, PTR(name), 0, 0) == 0);
    check(call(SYS_FS_READ, PTR(name), PTR(scratch), 4) == 0);
    /* Stress the full supervisor staging buffer and verify all bytes. */
    for (unsigned i = 0; i < 4096; i++) scratch[i] = (char)(i * 17);
    check(call(SYS_FS_WRITE, PTR(name), PTR(scratch), 4096) == 0);
    check(call(SYS_FS_READ, PTR(name), PTR(scratch + 4096), 4096) == 4096);
    for (unsigned i = 0; i < 4096; i++) check(scratch[i] == scratch[i + 4096]);
    check(call(SYS_FS_DELETE, PTR(name), 0, 0) == 0);
    asm volatile("std" ::: "cc");
    check(call(SYS_WRITE, PTR("syscall DF test\n"), 0, 0) == 0);
    asm volatile("cld" ::: "cc");
    test_completed = 1;
    call(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
#endif
