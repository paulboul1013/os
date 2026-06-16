#include "usermode.h"
#include "gdt.h"
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
        "pushf\n"          // EFLAGS（keep current flags）
        "orl $0x200, (%%esp)\n" // 確保 IF=1（允許中斷）
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
    //allocate user stack
    uint8_t *user_stack = (uint8_t*)kmalloc(USER_STACK_SIZE, 0, NULL);
    uint32_t user_esp =(uint32_t)(user_stack+USER_STACK_SIZE);


    enter_usermode((void*)entry,user_esp);
}