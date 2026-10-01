#ifdef PROTECTION_TEST
#include "../cpu/task.h"
#include "../cpu/usermode.h"
#include "../cpu/paging.h"
#include "../cpu/usercopy.h"
#include "../cpu/ports.h"
#include "../cpu/timer.h"
#include "../drivers/screen.h"
#include "../libc/mem.h"

extern volatile uint32_t test_mode, test_address, test_completed, test_survivor, test_last_stack;
extern void user_protection_test(void), user_test_survivor(void);
extern char __user_text_start, __user_rodata_start, __user_data_start, __user_data_end;
static volatile uint32_t faults;
extern volatile uint32_t tick;
static uint32_t expected_vector, expected_error, expected_address;
static uint32_t sentinel = 0x12345678;
static void require(int condition) {
    if (!condition) {
        kprint("KERNEL TEST FAILED\n");
        port_byte_out(0xf4, 0x11);
        for (;;) asm volatile("cli; hlt");
    }
}
void protection_fault_observed(registers_t *r, uint32_t cr2) {
    require((r->cs & 3) == 3);
    require(r->int_no == expected_vector);
    require(r->err_code == expected_error);
    if (r->int_no == 14) require(cr2 == expected_address);
    faults++;
}
static void run_user(void) { lauch_user_task(user_protection_test); }
static void survivor(void) { lauch_user_task(user_test_survivor); }
static volatile uint32_t returned;
static void returning_task(void) { returned = 1; }
static void wait_task(pcb_t *p) {
    uint32_t pid = p->pid;
    while (p->pid == pid && p->state != TASK_TERMINATED) task_yield();
    task_yield();
}
void protection_tests(void) {
    asm volatile("cli" ::: "memory");
    require(!user_range_valid((void*)0x8000, 1, 0));
    require(!user_range_valid((void*)0xfffff000, 1, 0));
    require(user_range_valid(&__user_text_start, 1, 0));
    require(!user_range_valid(&__user_text_start, 1, 1));
    require(!user_range_valid((void*)0xfffffff0, 32, 0));
    /* Audit all identity PTEs: no kernel page accidentally user-accessible. */
    for (uint32_t a = 0; a < 0x800000; a += 4096) {
        int expected = a >= (uint32_t)&__user_text_start && a < (uint32_t)&__user_data_end;
        require(paging_user_access(a, 0) == expected);
    }
    volatile uint32_t *pd = (uint32_t*)PAGE_DIRECTORY_BASE;
    uint32_t saved = pd[0];
    pd[0] &= ~4u;
    require(!user_range_valid(&__user_text_start, 1, 0));
    pd[0] = saved & ~2u;
    require(!user_range_valid(&__user_data_start, 1, 1));
    pd[0] = saved;
    asm volatile("sti" ::: "memory");
    pcb_t *p = task_create(run_user); require(p != 0); wait_task(p);
    require(test_completed == 1);
    require(!paging_user_access(test_last_stack, 0));
    p = task_create(returning_task); require(p != 0); wait_task(p);
    require(returned == 1);
    require(task_create(survivor) != 0);
    uint32_t targets[] = {(uint32_t)&sentinel, 0x100000, PAGE_METADATA_BASE,
        PAGE_DIRECTORY_BASE, task_current()->kernel_stack, 0x8fff0};
    uint32_t start_tick = tick;
    for (unsigned iteration = 0; iteration < 90; iteration++) {
        unsigned n = iteration % 18;
        test_mode = n < 12 ? 1 + n % 2 : n < 14 ? 2 : n == 14 ? 3 : n == 15 ? 4 : 1 + n % 2;
        test_address = n < 12 ? targets[n / 2] : n == 12 ? (uint32_t)&__user_text_start : (uint32_t)&__user_rodata_start;
        if (n >= 16) test_address = 0x800000;
        expected_vector = n < 14 || n >= 16 ? 14 : n == 14 ? 13 : 6;
        expected_error = n < 14 ? (test_mode == 1 ? 5 : 7) : n == 14 ? 0x102 : 0;
        if (n >= 16) expected_error = test_mode == 1 ? 4 : 6;
        expected_address = test_address;
        uint32_t before = faults, alive = test_survivor;
        p = task_create(run_user); require(p != 0); wait_task(p);
        require(faults == before + 1);
        require(!paging_user_access(test_last_stack, 0));
        for (unsigned i = 0; i < 4096; i++) require(((uint8_t*)test_last_stack)[i] == 0);
        require(test_survivor > alive);
        require(sentinel == 0x12345678);
    }
    while (tick == start_tick) task_yield();
    kprint("PROTECTION TESTS PASS: syscall matrix, 90 faults, survivor, timer, reuse\n");
    port_byte_out(0xf4, 0x10);
    for (;;) asm volatile("cli; hlt");
}
#endif
