#include "usermode.h"
#include "gdt.h"
#include "task.h"
#include "pmm.h"
#include "paging.h"
#include "../libc/mem.h"

#define USER_STACK_SIZE 4096

void enter_usermode(void *user_entry,uint32_t user_esp) {
    //setting data segment for user mode

    asm volatile(
        "movw %0, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        :: "i"(USER_DS)
        : "eax"
    );

    //fake iret back to stack，jump into ring 3
    asm volatile(
        "pushl %0\n"       // SS = User Data Selector
        "pushl %1\n"       // ESP = User stack pointer
        "pushl $0x202\n"
        "pushl %2\n"       // CS = User Code Selector
        "pushl %3\n"       // EIP = 用戶程式入口
        "iret\n"
        :: "i"(USER_DS),
           "r"(user_esp),
           "i"(USER_CS),
           "r"(user_entry)
    );
}


void lauch_user_task(void (*entry)(void)) {
    asm volatile("cli" ::: "memory");
    uint32_t stack = pmm_alloc_frame();
    if (!stack || stack >= 0x800000) {
        if (stack) pmm_free_frame(stack);
        task_exit();
    }
    task_current()->user_stack = stack;
    memory_set((uint8_t*)stack, 0, USER_STACK_SIZE);
    paging_set_user_stack(stack, 1);
    enter_usermode((void*)entry, stack + USER_STACK_SIZE);
    __builtin_unreachable();
}
