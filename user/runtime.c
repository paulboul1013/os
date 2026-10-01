#include <stdint.h>
/* Separate user guard: never access the supervisor runtime from Ring 3. */
uintptr_t __user_stack_chk_guard = 0x93b7e421;
__attribute__((noreturn))
void __user_stack_chk_fail(void) {
    asm volatile("int $0x80" :: "a"(0), "b"(-1) : "memory", "cc");
    for (;;) {}
}
