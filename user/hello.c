static inline void sys_write(const char *str) {
    asm volatile(
        "mov eax, 1\n"  // SYS_WRITE
        "mov ebx, %0\n" // 字串指標
        "int 0x80\n"
        :: "r"(str)
        : "eax", "ebx"
    );
}

static inline void sys_exit(int code) {
    asm volatile(
        "mov eax, 0\n"  // SYS_EXIT
        "mov ebx, %0\n" // 返回碼
        "int 0x80\n"
        :: "r"(code)
        : "eax", "ebx"
    );
}

void user_hello(void) {
    sys_write("Hello from Ring 3!\n");
    sys_write("I am running in user mode.\n");
    sys_exit(0);
    // 永遠不該跑到這裡
    while(1);
}