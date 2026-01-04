#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "screen.h"
#include "../libc/string.h"
#include "../libc/function.h"
#include "../kernel/kernel.h"
#include <stddef.h>

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define TAB 0x0F

static  char key_buffer[256];
static int input_line_start_offset = 0; // Track the start of current input line

// 可用命令列表（用於 Tab 自動補全）
static const char* available_commands[] = {
    "END",
    "PAGE",
    NULL  // 結束標記
};

// Tab 補全：找到匹配輸入的命令
// 返回匹配的命令，如果沒有匹配則返回 NULL
// 如果有多個匹配，返回第一個匹配的命令
static const char* find_command_match(char* input) {
    if (input == NULL || strlen(input) == 0) {
        return NULL;
    }
    
    int i = 0;
    while (available_commands[i] != NULL) {
        if (strstartswith(available_commands[i], input)) {
            return available_commands[i];
        }
        i++;
    }
    return NULL;
}

// 清除當前輸入行並顯示補全後的文字
// 這個函數會清除從 input_line_start_offset 到游標位置的所有文字
// 然後顯示補全後的命令
static void complete_and_display(const char* completed_command) {
    if (completed_command == NULL) {
        return; // 沒有匹配的命令，不做任何事
    }
    
    // 清除當前輸入行（從 input_line_start_offset 到游標位置）
    int chars_to_clear = strlen(key_buffer);
    
    // 用 backspace 清除所有已輸入的字元
    int i;
    for (i = 0; i < chars_to_clear; i++) {
        kprint_backspace(input_line_start_offset);
    }
    
    // 更新 key_buffer 為補全後的命令
    int j = 0;
    while (completed_command[j] != '\0' && j < 255) {
        key_buffer[j] = completed_command[j];
        j++;
    }
    key_buffer[j] = '\0';
    
    // 顯示補全後的命令
    kprint(key_buffer);
}

#define SC_MAX 57

const char *sc_name[] = {
    "ERROR", "Esc", "1", "2", "3", "4", "5", "6", 
    "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E", 
        "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl", 
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", 
        "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", 
        "/", "RShift", "Keypad *", "LAlt", "Spacebar"};

const char  sc_ascii[] ={
    '?', '?', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '-', '=', '?', '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '[', ']', '?', '?', 'A', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', ';', '\'', '`', '?', '\\', 'Z', 'X', 'C', 'V', 
        'B', 'N', 'M', ',', '.', '/', '?', '?', '?', ' '
};


static void keyboard_callback(registers_t *regs){
    //The PIC leaves us the scancode in port 0x60
    uint8_t scancode=port_byte_in(0x60);

    if (scancode > SC_MAX) return;
    if (scancode == BACKSPACE){
        // Only allow backspace if there are characters in buffer and cursor is past input line start
        if (strlen(key_buffer) > 0) {
            backspace(key_buffer);
            kprint_backspace(input_line_start_offset);
        }
    }else if (scancode == TAB){
        // Tab 自動補全
        const char* match = find_command_match(key_buffer);
        if (match != NULL) {
            complete_and_display(match);
        }
        // 如果沒有匹配，不做任何事（可以選擇發出提示音或顯示錯誤）
    }else if (scancode ==ENTER){
        kprint("\n");
        user_input(key_buffer);//kernel-controlled function
        key_buffer[0]='\0';
        // Update input line start to current cursor position (after prompt)
        input_line_start_offset = get_cursor_offset();
    } else{
        char letter=sc_ascii[(int)scancode];
        //kprint ony accepts char[]
        char str[2]={letter,'\0'};
        append(key_buffer,letter);
        kprint(str);
    }

    UNUSED(regs);
}



void init_keyboard() {
    register_interrupt_handler(IRQ1,keyboard_callback);
    // Set initial input line start to current cursor position (after prompt)
    input_line_start_offset = get_cursor_offset();
}