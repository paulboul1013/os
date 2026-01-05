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
#define UP_ARROW 0x48
#define DOWN_ARROW 0x50
#define E0_PREFIX 0xE0

// 命令歷史記錄
#define MAX_HISTORY 50
static char history[MAX_HISTORY][256];
static int history_count = 0;  // 歷史記錄總數
static int history_index = -1; // 當前瀏覽的歷史記錄索引（-1 表示當前輸入）
static char current_input[256] = ""; // 保存當前輸入（用於上下鍵瀏覽時恢復）

static  char key_buffer[256];
static int input_line_start_offset = 0; // Track the start of current input line
static int expecting_e0 = 0; // 標記是否正在等待 0xE0 後續的 scancode

// 可用命令列表（用於 Tab 自動補全）
static const char* available_commands[] = {
    "END",
    "PAGE",
    "CLEAR",
    "ECHO",
    "CALC",
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

// 清除當前輸入行並顯示新的文字
static void clear_and_display(const char* text) {
    // 清除當前輸入行
    int chars_to_clear = strlen(key_buffer);
    int i;
    for (i = 0; i < chars_to_clear; i++) {
        kprint_backspace(input_line_start_offset);
    }
    
    // 更新 key_buffer
    int j = 0;
    if (text != NULL) {
        while (text[j] != '\0' && j < 255) {
            key_buffer[j] = text[j];
            j++;
        }
    }
    key_buffer[j] = '\0';
    
    // 顯示新文字
    if (text != NULL && strlen(text) > 0) {
        kprint(key_buffer);
    }
}

// 將命令添加到歷史記錄
static void add_to_history(const char* command) {
    if (command == NULL || strlen(command) == 0) {
        return; // 不保存空命令
    }
    
    // 如果歷史記錄已滿，移除最舊的記錄
    if (history_count >= MAX_HISTORY) {
        int i;
        // 將所有記錄向前移動一位
        for (i = 0; i < MAX_HISTORY - 1; i++) {
            int j = 0;
            while (history[i+1][j] != '\0' && j < 255) {
                history[i][j] = history[i+1][j];
                j++;
            }
            history[i][j] = '\0';
        }
        history_count = MAX_HISTORY - 1;
    }
    
    // 添加新命令到歷史記錄末尾
    int i = 0;
    while (command[i] != '\0' && i < 255) {
        history[history_count][i] = command[i];
        i++;
    }
    history[history_count][i] = '\0';
    history_count++;
    
    // 重置歷史記錄索引
    history_index = -1;
}

// 瀏覽歷史記錄（向上）
static void navigate_history_up() {
    if (history_count == 0) {
        return; // 沒有歷史記錄
    }
    
    // 如果是第一次按上鍵，保存當前輸入
    if (history_index == -1) {
        int i = 0;
        while (key_buffer[i] != '\0' && i < 255) {
            current_input[i] = key_buffer[i];
            i++;
        }
        current_input[i] = '\0';
        history_index = history_count - 1;
    } else if (history_index > 0) {
        history_index--;
    }
    
    // 顯示歷史記錄
    clear_and_display(history[history_index]);
}

// 瀏覽歷史記錄（向下）
static void navigate_history_down() {
    if (history_count == 0) {
        return; // 沒有歷史記錄
    }
    
    if (history_index == -1) {
        return; // 已經在當前輸入
    }
    
    history_index++;
    
    if (history_index >= history_count) {
        // 回到當前輸入
        history_index = -1;
        clear_and_display(current_input);
    } else {
        // 顯示歷史記錄
        clear_and_display(history[history_index]);
    }
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

    // 處理 0xE0 前綴（特殊鍵如上下左右鍵）
    if (scancode == E0_PREFIX) {
        expecting_e0 = 1;
        UNUSED(regs);
        return;
    }
    
    // 如果正在等待 0xE0 後續的 scancode
    if (expecting_e0) {
        expecting_e0 = 0;
        
        // 處理上下鍵（只處理 make code，忽略 break code）
        if (scancode == UP_ARROW) {
            navigate_history_up();
        } else if (scancode == DOWN_ARROW) {
            navigate_history_down();
        }
        // 其他特殊鍵暫時忽略
        UNUSED(regs);
        return;
    }

    if (scancode > SC_MAX) return;
    if (scancode == BACKSPACE){
        // Only allow backspace if there are characters in buffer and cursor is past input line start
        if (strlen(key_buffer) > 0) {
            int was_browsing_history = (history_index != -1);
            
            // 如果正在瀏覽歷史記錄，先退出歷史瀏覽模式並恢復到當前輸入
            if (was_browsing_history) {
                history_index = -1;
                // 恢復到當前輸入（清除歷史記錄顯示）
                clear_and_display(current_input);
                // 現在 key_buffer 已經是 current_input
            }
            
            // 從緩衝區移除字元
            backspace(key_buffer);
            
            // 清除螢幕上的字元
            kprint_backspace(input_line_start_offset);
            
            // 如果之前正在瀏覽歷史記錄，需要更新 current_input 以保持同步
            if (was_browsing_history) {
                int i = 0;
                while (key_buffer[i] != '\0' && i < 255) {
                    current_input[i] = key_buffer[i];
                    i++;
                }
                current_input[i] = '\0';
            }
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
        // 保存命令到歷史記錄
        if (strlen(key_buffer) > 0) {
            add_to_history(key_buffer);
        }
        user_input(key_buffer);//kernel-controlled function
        key_buffer[0]='\0';
        current_input[0]='\0'; // 清除當前輸入緩存
        // Update input line start to current cursor position (after prompt)
        input_line_start_offset = get_cursor_offset();
        // 重置歷史記錄索引
        history_index = -1;
    } else{
        char letter=sc_ascii[(int)scancode];
        if (letter != '?') { // 只處理有效的 ASCII 字元
            // 添加到緩衝區並顯示在螢幕上
            char str[2]={letter,'\0'};
            append(key_buffer,letter);
            kprint(str);
            // 重置歷史記錄索引，因為用戶正在輸入新內容
            history_index = -1;
        }
    }

    UNUSED(regs);
}



void init_keyboard() {
    register_interrupt_handler(IRQ1,keyboard_callback);
    // Set initial input line start to current cursor position (after prompt)
    input_line_start_offset = get_cursor_offset();
}