#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../libc/cal.h"
#include "../libc/stdio.h"
#include "../drivers/rtc.h"
#include "../cpu/timer.h"
#include <stdint.h>

int constructor_test_val = 0;

__attribute__((constructor)) void test_constructor() {
    constructor_test_val = 42;
}

// 強制不進行內聯，確保函式有自己的 Call Stack 和 Return Address
__attribute__((noinline))
void trigger_overflow() {
    char buffer[10];
    int i = 0;
    kprint("Writing beyond buffer limits...\n");
    // 寫入遠超緩衝區的大小 (64 bytes)，確保能蓋過 Canary 和 Return Address
    while (i < 64) {
        buffer[i] = 'A';
        i++;
    }
    // 到這裡通常已經毀壞堆疊了，如果有觸發 SSP，函式回傳時會跳轉到 __stack_chk_fail
}

extern uintptr_t __stack_chk_guard;

void kernel_main(){
    // simple "randomness" by mixing some bits (could be improved)
    __stack_chk_guard = __stack_chk_guard ^ (uintptr_t)&kernel_main;
    
    isr_install();
    mem_init();
    if (constructor_test_val == 42) {
        kprint("Global Constructors: OK\n");
    } else {
        kprint("Global Constructors: FAILED\n");
    }
    kprint("> ");
    irq_install();
}

void user_input(char *input){
    if (strcmp(input,"end")==0){
        kprint("Stopping the CPU\n");
        asm volatile("hlt");
    }else if (strcmp(input,"page")==0){
        //get test kmalloc
        uint32_t phys_addr;
        uint32_t page=kmalloc(1000,1,&phys_addr);
        printf("Page: 0x%x, physical address: 0x%x\n", page, phys_addr);
        printf("Freeing page...\n");
        kfree((void*)page);
        kprint("> ");
    }else if (strcmp(input,"malloc")==0){
        uint32_t m1 = kmalloc(100, 0, NULL);
        uint32_t m2 = kmalloc(100, 0, NULL);
        printf("Allocated 100 bytes at: 0x%x\n", m1);
        printf("Allocated 100 bytes at: 0x%x\n", m2);
        printf("Freeing first block...\n");
        kfree((void*)m1);
        uint32_t m3 = kmalloc(100, 0, NULL);
        printf("Re-allocated 100 bytes at: 0x%x\n", m3);
        if (m1 == m3) printf("Test Passed: Memory reused!\n");
        else printf("Test Failed: Memory NOT reused!\n");
        kprint("> ");
    }else if (strcmp(input,"clear")==0){
        clear_screen();
        kprint("> ");
    }else if (strstartswith(input,"echo ")){
        const char* text = input + 5;
        kprint(text);
        kprint("\n> ");
    }else if (strstartswith(input,"calc ")){
        calc_command(input);
    }else if (strcmp(input, "time") == 0) {
        char time_str[64] = "";
        get_time(time_str);
        kprint(time_str);
        kprint("\n> ");
    }else if (strcmp(input,"sleep") == 0){
        sleep(1);
        kprint("> ");
    }else if (strcmp(input, "beep") == 0) {
        play_sound(200); // Lower frequency
        sleep_ms(40);    // Very short beep 
        nosound();
        kprint("> ");
    }else if (strcmp(input, "printf") == 0) {
        printf("Printf test:\n");
        printf("Char: %c\n", 'A');
        printf("String: %s\n", "Hello, world!");
        printf("Decimal: %d\n", -123);
        printf("Unsigned: %u\n", 456);
        printf("Hex: 0x%x\n", 0xDEADBEEF);
        printf("Percent: %%\n");
        kprint("> ");
    }else if (strcmp(input, "stack") == 0) {
        kprint("Starting stack smashing test...\n");
        trigger_overflow();
        kprint("Should not reach here if SSP works!\n> ");
    }
    else{
        kprint("> ");
    }

}