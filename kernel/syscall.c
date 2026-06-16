#include "syscall.h"
#include "../cpu/idt.h"
#include "../drivers/screen.h"
#include "../cpu/timer.h"
#include "../cpu/task.h"

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

        case SYS_GETPID:
            r->eax=task_current()->pid;
            break;

        case SYS_SLEEP:
            sleep(r->ebx);
            r->eax=0;
            break;

        default:
            kprint("[syscall] Unkown syscall: ");
            r->eax=(uint32_t)-1; //return -1 (error)
            break;

    }
}