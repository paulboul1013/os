#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include "../libc/cal.h"
#include <stdint.h>

void kernel_main(){
    isr_install();
    irq_install();

    kprint(">");
   
    // kprint("Type something, it will go through the kernel\n"
    //     "Type END to halt the CPU or PAGE to request a kmalloc()\n> ");
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
        // ECHO 命令：顯示後面的文字
        const char* text = input + 5; // 跳過 "ECHO "
        kprint(text);
        kprint("\n> ");
    }else if (strstartswith(input,"calc ")){
        // CALC 命令：簡單計算器
        calc_command(input);
    }else{
        // 其他情況只顯示提示符
        kprint("> ");
    }

}