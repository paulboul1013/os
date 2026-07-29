#ifndef SYSCALL_H
#define SYSCALL_H

#include "../cpu/isr.h"  // registers_t

//syscall number define
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_SLEEP   3
#define SYS_GETPID  4
#define SYS_CLEAR   5
#define SYS_YIELD   6
#define SYS_FS_CREATE 7
#define SYS_FS_LIST   8
#define SYS_FS_READ   9
#define SYS_FS_WRITE  10
#define SYS_FS_DELETE 11

// mount int 0x80
void syscall_init(void);

// call asm stub
void syscall_handler(registers_t *r);


#endif
