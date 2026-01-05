#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include <stdint.h>

// 將字串轉換為整數
static int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    int i = 0;
    
    // 跳過空白字元
    while (str[i] == ' ') i++;
    
    // 處理負號
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }
    
    // 轉換數字
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }
    
    return result * sign;
}

// 簡單計算器：解析 CALC <num1><op><num2> 格式
static void calc_command(const char* input) {
    // 跳過 "CALC "
    const char* expr = input + 5; // "CALC " 是 5 個字元
    
    // 跳過空白
    while (*expr == ' ') expr++;
    
    // 解析第一個數字
    int num1 = 0;
    int sign1 = 1;
    if (*expr == '-') {
        sign1 = -1;
        expr++;
    } else if (*expr == '+') {
        expr++;
    }
    
    while (*expr >= '0' && *expr <= '9') {
        num1 = num1 * 10 + (*expr - '0');
        expr++;
    }
    num1 *= sign1;
    
    // 跳過空白
    while (*expr == ' ') expr++;
    
    // 解析運算符
    char op = *expr;
    if (op != '+' && op != '-' && op != '*' && op != '/') {
        kprint("Error: Invalid operator\n> ");
        return;
    }
    expr++;
    
    // 跳過空白
    while (*expr == ' ') expr++;
    
    // 解析第二個數字
    int num2 = 0;
    int sign2 = 1;
    if (*expr == '-') {
        sign2 = -1;
        expr++;
    } else if (*expr == '+') {
        expr++;
    }
    
    while (*expr >= '0' && *expr <= '9') {
        num2 = num2 * 10 + (*expr - '0');
        expr++;
    }
    num2 *= sign2;
    
    // 執行計算
    int result = 0;
    if (op == '+') {
        result = num1 + num2;
    } else if (op == '-') {
        result = num1 - num2;
    } else if (op == '*') {
        result = num1 * num2;
    } else if (op == '/') {
        if (num2 == 0) {
            kprint("Error: Division by zero\n> ");
            return;
        }
        result = num1 / num2;
    }
    
    // 顯示結果
    char result_str[16] = "";
    int_to_ascii(result, result_str);
    kprint(result_str);
    kprint("\n> ");
}

void kernel_main(){
    isr_install();
    irq_install();

   
    kprint("Type something, it will go through the kernel\n"
        "Type END to halt the CPU or PAGE to request a kmalloc()\n> ");
}

void user_input(char *input){
    if (strcmp(input,"END")==0){
        kprint("Stopping the CPU\n");
        asm volatile("hlt");
    }else if (strcmp(input,"PAGE")==0){
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
    }else if (strcmp(input,"CLEAR")==0){
        clear_screen();
        kprint("> ");
    }else if (strstartswith(input,"ECHO ")){
        // ECHO 命令：顯示後面的文字
        const char* text = input + 5; // 跳過 "ECHO "
        kprint(text);
        kprint("\n> ");
    }else if (strstartswith(input,"CALC ")){
        // CALC 命令：簡單計算器
        calc_command(input);
    }else{
        // 其他情況只顯示提示符
        kprint("> ");
    }

}