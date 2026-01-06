#ifndef CAL_H
#define CAL_H

// 將字串轉換為整數
int atoi(const char* str);

// 簡單計算器：解析 CALC <num1><op><num2> 格式
void calc_command(const char* input);

#endif

