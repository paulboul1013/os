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
        kprint("\n> ");
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
    }else{
        kprint("> ");
    }

}