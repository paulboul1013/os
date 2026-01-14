#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../libc/cal.h"
#include "../drivers/rtc.h"
#include "../cpu/timer.h"
#include <stdint.h>

void kernel_main(){
    isr_install();
    mem_init();
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
        char page_str[16]="";
        hex_to_ascii(page,page_str);
        char phys_str[16]="";
        hex_to_ascii(phys_addr,phys_str);
        kprint("Page: ");
        kprint(page_str);
        kprint(", physical address: ");
        kprint(phys_str);
        kprint("\nFreeing page...\n");
        kfree((void*)page);
        kprint("> ");
    }else if (strcmp(input,"malloc")==0){
        uint32_t m1 = kmalloc(100, 0, NULL);
        uint32_t m2 = kmalloc(100, 0, NULL);
        char s1[16]; hex_to_ascii(m1, s1);
        char s2[16]; hex_to_ascii(m2, s2);
        kprint("Allocated 100 bytes at: "); kprint(s1); kprint("\n");
        kprint("Allocated 100 bytes at: "); kprint(s2); kprint("\n");
        kprint("Freeing first block...\n");
        kfree((void*)m1);
        uint32_t m3 = kmalloc(100, 0, NULL);
        char s3[16]; hex_to_ascii(m3, s3);
        kprint("Re-allocated 100 bytes at: "); kprint(s3); kprint("\n");
        if (m1 == m3) kprint("Test Passed: Memory reused!\n");
        else kprint("Test Failed: Memory NOT reused!\n");
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
    }
    else{
        kprint("> ");
    }

}