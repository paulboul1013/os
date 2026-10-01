#include "syscall.h"
#include "../cpu/idt.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../cpu/timer.h"
#include "../cpu/task.h"
#include "../cpu/scheduler.h"
#include "../fs/fs.h"
#include "../cpu/usercopy.h"

extern void syscall_stub(void);

void syscall_init(void){
    set_idt_gate(0x80,(uint32_t)syscall_stub);
}

/* Buffers live on each task's supervisor stack, never in shared user memory. */
static int file_syscall(registers_t *r) {
    char name[FS_MAX_FILENAME];
    uint8_t data[FS_MAX_FILESIZE];
    int result = copy_string_from_user(name, (const char*)r->ebx, sizeof(name));
    if (result) return result;
    switch (r->eax) {
        case SYS_FS_CREATE: return fs_create(name);
        case SYS_FS_DELETE: return fs_delete(name);
        case SYS_FS_WRITE:
            if (!user_range_valid((void*)r->ecx, r->edx, 0)) return USER_EFAULT;
            if (r->edx > sizeof(data)) return FS_ERR_OVERFLOW;
            result = copy_from_user(data, (void*)r->ecx, r->edx);
            return result ? result : fs_write(name, data, r->edx);
        default:
            if (!user_range_valid((void*)r->ecx, r->edx, 1)) return USER_EFAULT;
            result = fs_read(name, data, r->edx < sizeof(data) ? r->edx : sizeof(data));
            if (result < 0) return result;
            int copied = copy_to_user((void*)r->ecx, data, result);
            return copied ? copied : result;
    }
}

void syscall_handler(registers_t *r) {
    switch (r->eax) {
        case SYS_EXIT: task_exit();
        case SYS_WRITE: {
            char text[USER_STRING_MAX];
            int error = copy_string_from_user(text, (const char*)r->ebx, sizeof(text));
            if (!error) kprint(text);
            r->eax = error;
            break;
        }
        case SYS_READ: {
            uint32_t len = r->ecx;
            if (!len) { r->eax = (uint32_t)-1; break; }
            if (!user_range_valid((void*)r->ebx, len, 1)) {
                r->eax = USER_EFAULT;
                break;
            }
            char text[256];
            int count = keyboard_read_line(text, len < sizeof(text) ? len : sizeof(text));
            if (count < 0) { r->eax = (uint32_t)count; break; }
            int error = copy_to_user((void*)r->ebx, text, count + 1);
            r->eax = error ? (uint32_t)error : (uint32_t)count;
            break;
        }
        case SYS_GETPID: r->eax = task_current()->pid; break;
        case SYS_SLEEP:
            r->eax = (uint32_t)sleep(r->ebx);
            break;
        case SYS_CLEAR: clear_screen(); r->eax = 0; break;
        case SYS_YIELD: task_yield(); r->eax = 0; break;
        case SYS_FS_LIST: fs_list(); r->eax = 0; break;
        case SYS_FS_CREATE:
        case SYS_FS_READ:
        case SYS_FS_WRITE:
        case SYS_FS_DELETE: r->eax = file_syscall(r); break;
        default: r->eax = (uint32_t)-1; break;
    }
}
