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
#define LEFT_ARROW 0x4B
#define RIGHT_ARROW 0x4D
#define E0_PREFIX 0xE0
#define LSHIFT 0x2A
#define RSHIFT 0x36

// 命令歷史記錄
#define MAX_HISTORY 50
static char history[MAX_HISTORY][256];
static int history_count = 0;  // 歷史記錄總數
static int history_index = -1; // 當前瀏覽的歷史記錄索引（-1 表示當前輸入）
static char current_input[256] = ""; // 保存當前輸入（用於上下鍵瀏覽時恢復）

static  char key_buffer[256];
static int input_line_start_offset = 0; // Track the start of current input line
static int expecting_e0 = 0; // 標記是否正在等待 0xE0 後續的 scancode
static int cursor_position = 0; // 光標在 key_buffer 中的位置（0 = 開頭，strlen = 末尾）
static int shift_active = 0; // 標記 Shift 鍵是否被按下

// 可用命令列表（用於 Tab 自動補全）
static const char* available_commands[] = {
    "END",
    "PAGE",
    "CLEAR",
    "ECHO",
    "CALC",
    NULL  // 結束標記
};

// 在光標位置插入字符
static void insert_char_at_cursor(char c) {
    int len = strlen(key_buffer);
    if (len >= 255) return; // 緩衝區已滿
    
    // 將光標位置之後的字符向後移動
    int i;
    for (i = len; i > cursor_position; i--) {
        key_buffer[i] = key_buffer[i-1];
    }
    key_buffer[cursor_position] = c;
    key_buffer[len + 1] = '\0';
    cursor_position++;
}

// 在光標位置刪除字符（刪除光標前的字符，類似 backspace）
static void delete_char_at_cursor() {
    int len = strlen(key_buffer);
    if (cursor_position <= 0 || len == 0) return;
    
    // 刪除 cursor_position - 1 位置的字符
    // 將從 cursor_position 開始的所有字符向前移動一位
    // 例如："HELLO"，cursor_position=5（在最後），刪除位置4的'O'
    // 結果："HELL\0"
    int i;
    for (i = cursor_position - 1; i < len; i++) {
        key_buffer[i] = key_buffer[i+1];  // i+1 在 i=len-1 時是 '\0'，這是正確的
    }
    // 光標位置減1
    cursor_position--;
}

// 重新顯示整個輸入行（用於光標移動後）
// old_displayed_len: 刪除前屏幕上顯示的字符數（用於清除），如果為 -1 則使用當前 key_buffer 長度
static void redisplay_input_line(int old_displayed_len) {
    // 確保 cursor_position 不超過 key_buffer 的長度
    int len = strlen(key_buffer);
    if (cursor_position > len) {
        cursor_position = len;
    }
    
    // 計算需要清除的字符數
    int chars_to_clear;
    if (old_displayed_len >= 0) {
        // 使用傳入的舊長度
        chars_to_clear = old_displayed_len;
    } else {
        // 使用當前 key_buffer 長度
        chars_to_clear = len;
    }
    
    // 計算起始位置的行列
    int start_row = input_line_start_offset / (2 * MAX_COLS);
    int start_col = (input_line_start_offset - start_row * 2 * MAX_COLS) / 2;
    
    // 檢查起始位置是否在有效範圍內
    if (start_row >= 0 && start_row < MAX_ROWS && start_col >= 0 && start_col < MAX_COLS) {
        // 先將光標設置到要清除區域的末尾（如果有的話）
        // 然後從右到左清除所有已顯示的字符
        if (chars_to_clear > 0) {
            // 將光標設置到清除區域的末尾
            int clear_end_offset = input_line_start_offset + chars_to_clear * 2;
            set_cursor_offset(clear_end_offset);
            
            // 從右到左清除字符
            int i;
            for (i = 0; i < chars_to_clear; i++) {
                kprint_backspace(input_line_start_offset);
            }
        }
        
        // 確保光標在 input_line_start_offset
        set_cursor_offset(input_line_start_offset);
        
        // 顯示 key_buffer
        if (len > 0) {
            kprint(key_buffer);
        }
        
        // 移動光標到正確位置
        int target_offset = input_line_start_offset + cursor_position * 2;
        
        // 檢查目標偏移是否超出屏幕範圍
        int max_offset = MAX_COLS * MAX_ROWS * 2;
        if (target_offset < 0) {
            target_offset = 0;
        } else if (target_offset >= max_offset) {
            target_offset = max_offset - 2;
        }
        
        // 計算目標位置的行列並檢查
        int target_row = target_offset / (2 * MAX_COLS);
        int target_col = (target_offset - target_row * 2 * MAX_COLS) / 2;
        if (target_row >= 0 && target_row < MAX_ROWS && target_col >= 0 && target_col < MAX_COLS) {
            set_cursor_offset(target_offset);
        }
    }
}

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
    
    // 確保光標在 input_line_start_offset
    set_cursor_offset(input_line_start_offset);
    
    // 更新 key_buffer 為補全後的命令
    int j = 0;
    while (completed_command[j] != '\0' && j < 255) {
        key_buffer[j] = completed_command[j];
        j++;
    }
    key_buffer[j] = '\0';
    
    // 設置光標位置到末尾
    cursor_position = strlen(key_buffer);
    
    // 計算起始位置的行列，確保不會超出範圍
    int start_row = input_line_start_offset / (2 * MAX_COLS);
    int start_col = (input_line_start_offset - start_row * 2 * MAX_COLS) / 2;
    
    // 檢查起始位置是否在有效範圍內
    if (start_row >= 0 && start_row < MAX_ROWS && start_col >= 0 && start_col < MAX_COLS) {
        // 顯示補全後的命令
        kprint(key_buffer);
        
        // 移動光標到末尾
        int target_offset = input_line_start_offset + cursor_position * 2;
        
        // 檢查目標偏移是否超出屏幕範圍
        int max_offset = MAX_COLS * MAX_ROWS * 2;
        if (target_offset < 0) {
            target_offset = 0;
        } else if (target_offset >= max_offset) {
            target_offset = max_offset - 2;
        }
        
        // 計算目標位置的行列並檢查
        int target_row = target_offset / (2 * MAX_COLS);
        int target_col = (target_offset - target_row * 2 * MAX_COLS) / 2;
        if (target_row >= 0 && target_row < MAX_ROWS && target_col >= 0 && target_col < MAX_COLS) {
            set_cursor_offset(target_offset);
        }
    }
}

// 清除當前輸入行並顯示新的文字
static void clear_and_display(const char* text) {
    // 清除當前輸入行
    int chars_to_clear = strlen(key_buffer);
    int i;
    for (i = 0; i < chars_to_clear; i++) {
        kprint_backspace(input_line_start_offset);
    }
    
    // 確保光標在 input_line_start_offset
    set_cursor_offset(input_line_start_offset);
    
    // 更新 key_buffer
    int j = 0;
    if (text != NULL) {
        while (text[j] != '\0' && j < 255) {
            key_buffer[j] = text[j];
            j++;
        }
    }
    key_buffer[j] = '\0';
    
    // 設置光標位置到末尾
    cursor_position = strlen(key_buffer);
    
    // 計算起始位置的行列，確保不會超出範圍
    int start_row = input_line_start_offset / (2 * MAX_COLS);
    int start_col = (input_line_start_offset - start_row * 2 * MAX_COLS) / 2;
    
    // 檢查起始位置是否在有效範圍內
    if (start_row >= 0 && start_row < MAX_ROWS && start_col >= 0 && start_col < MAX_COLS) {
        // 顯示新文字
        if (text != NULL && strlen(text) > 0) {
            kprint(key_buffer);
        }
        
        // 移動光標到末尾
        int target_offset = input_line_start_offset + cursor_position * 2;
        
        // 檢查目標偏移是否超出屏幕範圍
        int max_offset = MAX_COLS * MAX_ROWS * 2;
        if (target_offset < 0) {
            target_offset = 0;
        } else if (target_offset >= max_offset) {
            target_offset = max_offset - 2;
        }
        
        // 計算目標位置的行列並檢查
        int target_row = target_offset / (2 * MAX_COLS);
        int target_col = (target_offset - target_row * 2 * MAX_COLS) / 2;
        if (target_row >= 0 && target_row < MAX_ROWS && target_col >= 0 && target_col < MAX_COLS) {
            set_cursor_offset(target_offset);
        }
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
    '7', '8', '9', '0', '-', '=', '?', '?', 'q', 'w', 'e', 'r', 't', 'y', 
        'u', 'i', 'o', 'p', '[', ']', '?', '?', 'a', 's', 'd', 'f', 'g', 
        'h', 'j', 'k', 'l', ';', '\'', '`', '?', '\\', 'z', 'x', 'c', 'v', 
        'b', 'n', 'm', ',', '.', '/', '?', '?', '?', ' '
};

const char sc_ascii_shift[] = {
    '?', '?', '!', '@', '#', '$', '%', '^',     
    '&', '*', '(', ')', '_', '+', '?', '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '{', '}', '?', '?', 'A', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', ':', '\"', '~', '?', '|', 'Z', 'X', 'C', 'V', 
        'B', 'N', 'M', '<', '>', '?', '?', '?', '?', ' '
};


static void keyboard_callback(registers_t *regs){
    //The PIC leaves us the scancode in port 0x60
    uint8_t scancode=port_byte_in(0x60);

    // 處理 Shift 鍵的按下 (Make code)
    if (scancode == LSHIFT || scancode == RSHIFT) {
        shift_active = 1;
        UNUSED(regs);
        return;
    }
    
    // 處理 Shift 鍵的放開 (Break code)
    if (scancode == (LSHIFT | 0x80) || scancode == (RSHIFT | 0x80)) {
        shift_active = 0;
        UNUSED(regs);
        return;
    }

    // 處理 0xE0 前綴（特殊鍵如上下左右鍵）
    if (scancode == E0_PREFIX) {
        expecting_e0 = 1;
        UNUSED(regs);
        return;
    }
    
    // 如果正在等待 0xE0 後續的 scancode
    if (expecting_e0) {
        expecting_e0 = 0;
        
        // 如果正在瀏覽歷史記錄，開始編輯時退出歷史瀏覽模式
        if (history_index != -1) {
            // 保存當前編輯的內容到 current_input
            int i = 0;
            while (key_buffer[i] != '\0' && i < 255) {
                current_input[i] = key_buffer[i];
                i++;
            }
            current_input[i] = '\0';
            // 退出歷史瀏覽模式，進入正常編輯模式
            history_index = -1;
        }
        
        // 處理上下左右鍵（只處理 make code，忽略 break code）
        if (scancode == UP_ARROW) {
            navigate_history_up();
        } else if (scancode == DOWN_ARROW) {
            navigate_history_down();
        } else if (scancode == LEFT_ARROW) {
            // 左鍵：光標向左移動
            if (cursor_position > 0) {
                cursor_position--;
                int target_offset = input_line_start_offset + cursor_position * 2;
                // 檢查目標偏移是否超出屏幕範圍
                int max_offset = MAX_COLS * MAX_ROWS * 2;
                if (target_offset < 0) {
                    target_offset = 0;
                } else if (target_offset >= max_offset) {
                    target_offset = max_offset - 2;
                }
                set_cursor_offset(target_offset);
            }
        } else if (scancode == RIGHT_ARROW) {
            // 右鍵：光標向右移動
            int len = strlen(key_buffer);
            if (cursor_position < len) {
                cursor_position++;
                int target_offset = input_line_start_offset + cursor_position * 2;
                // 檢查目標偏移是否超出屏幕範圍
                int max_offset = MAX_COLS * MAX_ROWS * 2;
                if (target_offset < 0) {
                    target_offset = 0;
                } else if (target_offset >= max_offset) {
                    target_offset = max_offset - 2;
                }
                set_cursor_offset(target_offset);
            }
        }
        // 其他特殊鍵暫時忽略
        UNUSED(regs);
        return;
    }

    if (scancode > SC_MAX) return;
    if (scancode == BACKSPACE){
        // Only allow backspace if there are characters in buffer and cursor is past input line start
        if (strlen(key_buffer) > 0 && cursor_position > 0) {
            int was_browsing_history = (history_index != -1);
            
            // 如果正在瀏覽歷史記錄，開始編輯時退出歷史瀏覽模式
            if (was_browsing_history) {
                // 保存當前編輯的內容到 current_input（因為用戶開始編輯歷史記錄）
                int i = 0;
                while (key_buffer[i] != '\0' && i < 255) {
                    current_input[i] = key_buffer[i];
                    i++;
                }
                current_input[i] = '\0';
                // 退出歷史瀏覽模式，進入正常編輯模式
                history_index = -1;
            }
            
            // 保存刪除前的長度，用於清除屏幕
            int old_len = strlen(key_buffer);
            
            // 在光標位置刪除字符
            delete_char_at_cursor();
            
            // 重新顯示輸入行（傳入舊長度用於清除）
            redisplay_input_line(old_len);
            
            // 如果之前正在瀏覽歷史記錄，同步更新 current_input
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
        cursor_position = 0; // 重置光標位置
        // Update input line start to current cursor position (after prompt)
        input_line_start_offset = get_cursor_offset();
        // 重置歷史記錄索引
        history_index = -1;
    } else{
        char letter;
        if (shift_active) {
            letter = sc_ascii_shift[(int)scancode];
        } else {
            letter = sc_ascii[(int)scancode];
        }
        
        if (letter != '?') { // 只處理有效的 ASCII 字元
            int was_browsing_history = (history_index != -1);
            
            // 如果正在瀏覽歷史記錄，開始編輯時退出歷史瀏覽模式
            if (was_browsing_history) {
                // 保存當前編輯的內容到 current_input（因為用戶開始編輯歷史記錄）
                int i = 0;
                while (key_buffer[i] != '\0' && i < 255) {
                    current_input[i] = key_buffer[i];
                    i++;
                }
                current_input[i] = '\0';
                // 退出歷史瀏覽模式，進入正常編輯模式
                history_index = -1;
            }
            
            // 在光標位置插入字符
            insert_char_at_cursor(letter);
            
            // 重新顯示輸入行（傳入 -1 使用當前長度）
            redisplay_input_line(-1);
            
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