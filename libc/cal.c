#include "cal.h"
#include "string.h"
#include "../drivers/screen.h"

// 將字串轉換為整數
int atoi(const char* str) {
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
void calc_command(const char* input) {
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

