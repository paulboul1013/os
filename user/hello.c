static inline void sys_write(const char *str) {
    asm volatile(
        "movl $1, %%eax\n"  // SYS_WRITE
        "movl %0, %%ebx\n" // 字串指標
        "int $0x80\n"
        :: "r"(str)
        : "eax", "ebx", "memory", "cc"
    );
}

static inline void sys_exit(int code) {
    asm volatile(
        "movl $0, %%eax\n"  // SYS_EXIT
        "movl %0, %%ebx\n" // 返回碼
        "int $0x80\n"
        :: "r"(code)
        : "eax", "ebx", "memory", "cc"
    );
}

void user_hello(void) {
    sys_write("Hello from Ring 3!\n");
    sys_write("I am running in user mode.\n");
    sys_exit(0);
    // 永遠不該跑到這裡
    while(1);
}