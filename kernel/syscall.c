#include "syscall.h"
#include "../cpu/idt.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../cpu/timer.h"
#include "../cpu/task.h"
#include "../cpu/scheduler.h"
#include "../fs/fs.h"

extern void syscall_stub(void);

void syscall_init(void){
    set_idt_gate(0x80,(uint32_t)syscall_stub);
}

void syscall_handler(registers_t *r){
    uint32_t syscall_num = r->eax;

    switch (syscall_num) {
        case SYS_EXIT:
            kprint("[syscall] exit()\n");
            task_exit();
            break;

        case SYS_WRITE: {
            char *str=(char*)(uintptr_t)r->ebx;
            kprint(str);
            r->eax=0;
            break;
        }

        case SYS_READ: {
            char *buf = (char*)(uintptr_t)r->ebx;
            uint32_t len = r->ecx;
            if (!buf || len == 0) {
                r->eax = (uint32_t)-1;
                break;
            }

            keyboard_prepare_input_line();
            scheduler_disable();
            while (!kbd_line_ready) {
                asm volatile("sti; hlt; cli");
            }
            scheduler_enable();

            uint32_t i = 0;
            while (i + 1 < len && kbd_line_buffer[i] != '\0') {
                buf[i] = kbd_line_buffer[i];
                i++;
            }
            buf[i] = '\0';
            kbd_line_ready = 0;
            kbd_line_buffer[0] = '\0';
            r->eax = i;
            break;
        }

        case SYS_GETPID:
            r->eax=task_current()->pid;
            break;

        case SYS_SLEEP:
            sleep(r->ebx);
            r->eax=0;
            break;

        case SYS_CLEAR:
            clear_screen();
            r->eax = 0;
            break;

        case SYS_YIELD:
            task_yield();
            r->eax = 0;
            break;

        case SYS_FS_CREATE:
            r->eax = fs_create((const char*)(uintptr_t)r->ebx);
            break;

        case SYS_FS_LIST:
            fs_list();
            r->eax = 0;
            break;

        case SYS_FS_READ:
            r->eax = fs_read((const char*)(uintptr_t)r->ebx,
                             (uint8_t*)(uintptr_t)r->ecx,
                             r->edx);
            break;

        case SYS_FS_WRITE:
            r->eax = fs_write((const char*)(uintptr_t)r->ebx,
                              (const uint8_t*)(uintptr_t)r->ecx,
                              r->edx);
            break;

        case SYS_FS_DELETE:
            r->eax = fs_delete((const char*)(uintptr_t)r->ebx);
            break;

        default:
            kprint("[syscall] Unknown syscall\n");
            r->eax=(uint32_t)-1; //return -1 (error)
            break;

    }
}
