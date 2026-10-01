#ifdef SCHED_TEST
#include <stdint.h>
#include "../kernel/syscall.h"

volatile uint32_t spin_a, spin_b, spin_stop, spin_bad;
volatile uint32_t spin_pid_a, spin_pid_b;
volatile uint32_t sleep_user_stage, sleep_user_bad;
volatile uint32_t read_user_stage;
volatile int read_user_result;
char read_target[256] __attribute__((aligned(4096)));

/* All work before spin_stop is pure Ring 3 computation: no yield or syscall. */
#define SPIN(counter, value, pid_slot) do {                                 \
    asm volatile(                                                           \
        "xorl %%eax, %%eax\n"                                              \
        "movw %%ax, %%es\n"                                                \
        "movw %%ax, %%fs\n"                                                \
        "movw %%ax, %%gs\n"                                                \
        "movl %%esp, %%edi\n"                                               \
        "movl $" value ", %%ebx\n"                                         \
        "std\n"                                                            \
        "1: incl (%[count])\n"                                              \
        "cmpl $0, (%[stop])\n"                                              \
        "jne 2f\n"                                                         \
        "cmpl $" value ", %%ebx\n"                                         \
        "jne 3f\n"                                                         \
        "cmpl %%edi, %%esp\n"                                              \
        "jne 3f\n"                                                         \
        "pushfl\n"                                                         \
        "popl %%eax\n"                                                     \
        "andl $0x600, %%eax\n"                                             \
        "cmpl $0x600, %%eax\n"                                             \
        "jne 3f\n"                                                         \
        "movw %%ds, %%ax\n"                                                \
        "cmpw $0x2b, %%ax\n"                                               \
        "jne 3f\n"                                                         \
        "movw %%es, %%ax\n"                                                \
        "testw %%ax, %%ax\n"                                               \
        "jne 3f\n"                                                         \
        "movw %%fs, %%ax\n"                                                \
        "testw %%ax, %%ax\n"                                               \
        "jne 3f\n"                                                         \
        "movw %%gs, %%ax\n"                                                \
        "testw %%ax, %%ax\n"                                               \
        "jne 3f\n"                                                         \
        "jmp 1b\n"                                                         \
        "3: movl $1, (%[bad])\n"                                           \
        "2: cld\n"                                                         \
        : : [count] "r" (&counter), [stop] "r" (&spin_stop),                \
            [bad] "r" (&spin_bad)                                           \
        : "eax", "ebx", "edi", "memory", "cc");                            \
    uint32_t pid;                                                             \
    asm volatile("int $0x80" : "=a"(pid) : "a"(SYS_GETPID) : "memory", "cc"); \
    uint16_t ds, es, fs, gs;                                                   \
    asm volatile("movw %%ds, %0; movw %%es, %1; movw %%fs, %2; movw %%gs, %3" \
        : "=r"(ds), "=r"(es), "=r"(fs), "=r"(gs));                              \
    if (ds != 0x2b || es || fs || gs) spin_bad = 1;                            \
    pid_slot = pid;                                                            \
    asm volatile("int $0x80" :: "a"(SYS_EXIT) : "memory", "cc");            \
    for (;;) {}                                                              \
} while (0)

void user_spin_a(void) { SPIN(spin_a, "0x13579bdf", spin_pid_a); }
void user_spin_b(void) { SPIN(spin_b, "0x2468ace0", spin_pid_b); }

void user_sleep_probe(void) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SLEEP), "b"(0) : "memory", "cc");
    if (result != 0) sleep_user_bad = 1;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SLEEP), "b"(0xffffffffu) : "memory", "cc");
    if (result != -1) sleep_user_bad = 1;
    sleep_user_stage = 1;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SLEEP), "b"(1) : "memory", "cc");
    if (result != 0) sleep_user_bad = 1;
    sleep_user_stage = 2;
    asm volatile("int $0x80" :: "a"(SYS_EXIT) : "memory", "cc");
    for (;;) {}
}

void user_read_probe(void) {
    read_user_stage = 1;
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_READ), "b"(read_target), "c"(sizeof(read_target)) : "memory", "cc");
    read_user_result = result;
    read_user_stage = 2;
    asm volatile("int $0x80" :: "a"(SYS_EXIT) : "memory", "cc");
    for (;;) {}
}
#endif
