#ifndef SYSCALL_H
#define SYSCALL_H

//syscall number define
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SLEEP 3
#define SYS_GETPID 4

// mount int 0x80
void syscall_init(void);

// call asm stub
void syscall_handler(registers_t *r);


#endif