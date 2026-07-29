// 這個檔案代表一個完整在 Ring 3 (User mode) 執行的 Shell。
// 大部分的 C standard function 不能直接使用，必須透過 System Call 請求 Kernel 協助。
#include <stdint.h>

static inline void sys_write(const char *str) {
    asm volatile(
        "movl $1, %%eax\n"
        "movl %0, %%ebx\n"
        "int $0x80\n"
        :: "r"(str)
        : "eax", "ebx"
    );
}

static inline int sys_read(char *buf, int len) {
    int ret;
    asm volatile(
        "movl $2, %%eax\n"
        "movl %1, %%ebx\n"
        "movl %2, %%ecx\n"
        "int $0x80\n"
        "movl %%eax, %0\n"
        : "=r"(ret)
        : "r"(buf), "r"(len)
        : "eax", "ebx", "ecx"
    );
    return ret;
}

static inline void sys_exit(int code) {
    asm volatile(
        "movl $0, %%eax\n"
        "movl %0, %%ebx\n"
        "int $0x80\n"
        :: "r"(code)
        : "eax", "ebx"
    );
}

static inline void sys_clear(void) {
    asm volatile(
        "movl $5, %%eax\n"
        "int $0x80\n"
        ::: "eax"
    );
}

static inline int sys_getpid(void) {
    int ret;
    asm volatile(
        "movl $4, %%eax\n"
        "int $0x80\n"
        "movl %%eax, %0\n"
        : "=r"(ret)
        :: "eax"
    );
    return ret;
}

// User-space strcmp
static int user_strcmp(const char *s1, const char *s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// User-space itoa
static void user_itoa(int n, char s[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    // reverse
    int j = 0, k = i - 1;
    while (j < k) {
        char c = s[j]; s[j] = s[k]; s[k] = c;
        j++; k--;
    }
}


void user_shell_main(void) {
    char buf[256];
    
    sys_clear();
    sys_write("\n===============================\n");
    sys_write("    OS Ring 3 User Shell\n");
    sys_write("===============================\n");

    while (1) {
        sys_write("user@os> ");
        
        sys_read(buf, 256); // Block until ENTER is pressed
        
        if (buf[0] == '\0') {
            continue;
        }

        if (user_strcmp(buf, "help") == 0 || user_strcmp(buf, "?") == 0) {
            sys_write("Commands: help, clear, pid, echo <text>, exit\n");
        } else if (user_strcmp(buf, "clear") == 0) {
            sys_clear();
        } else if (user_strcmp(buf, "pid") == 0) {
            int pid = sys_getpid();
            char pid_str[16];
            user_itoa(pid, pid_str);
            sys_write("Current Shell Task PID: ");
            sys_write(pid_str);
            sys_write("\n");
        } else if (user_strcmp(buf, "exit") == 0) {
            sys_write("Goodbye!\n");
            sys_exit(0);
        } else if (buf[0] == 'e' && buf[1] == 'c' && buf[2] == 'h' && buf[3] == 'o' && buf[4] == ' ') {
            sys_write(&buf[5]);
            sys_write("\n");
        } else {
            sys_write("Unknown command: ");
            sys_write(buf);
            sys_write("\nType 'help' for a list of commands.\n");
        }
    }
}
