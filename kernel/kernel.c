#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "syscall.h"
#include "../libc/mem.h"
#include "../cpu/pmm.h"
#include "../cpu/paging.h"
#include "../cpu/task.h"
#include "../cpu/scheduler.h"
#include "../cpu/usermode.h"
#include "../cpu/tss.h"
#include "../cpu/gdt.h"
#include "../fs/fs.h"
#include <stdint.h>



int constructor_test_val = 0;

__attribute__((constructor)) void test_constructor() {
    constructor_test_val = 42;
}

extern uintptr_t __stack_chk_guard;

void kernel_main(){
    // simple "randomness" by mixing some bits (could be improved)
    __stack_chk_guard = __stack_chk_guard ^ (uintptr_t)&kernel_main;
    
    isr_install();
    
    // 初始化 PMM (假設 128MB)
    pmm_init(128 * 1024 * 1024);
    // 保留內核佔用的內存 (1MB 內的安全區)
    pmm_reserve_region(0, 0x100000); 
    
    mem_init();
    
    // 初始化分頁
    init_paging();

    if (constructor_test_val == 42) {
        kprint("Global Constructors: OK\n");
    } else {
        kprint("Global Constructors: FAILED\n");
    }

    irq_install();
    syscall_init(); // 初始化系統呼叫

    // Initialize kernel GDT (replaces boot GDT, adds TSS entry)
    gdt_init();

    // Initialize TSS (kernel data segment 0x10, kernel stack at 0x90000)
    tss_init(0x10, 0x90000);
    tss_flush();

    // Initialize multitasking system
    task_init();

    // Initialize file system
    fs_init();

    kprint("Initializing Multitasking scheduler and User Space Shell...\n");
    // 新增一個 Shell Wrapper Task，當它被排程到時，就會切換進 Ring 3
    extern void user_shell_main(void);
    void shell_init_wrapper(void) {
        lauch_user_task(user_shell_main);
    }
    
    task_create(shell_init_wrapper);
    scheduler_enable(); // 啟動排程器（底層由 PIT Timer Driver 推動）

    // 讓原來的 kernel_main 退化為 Idle Process
    while (1) {
        asm volatile("hlt");
    }
}
