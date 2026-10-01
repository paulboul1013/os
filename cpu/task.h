#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define KERNEL_STACK_SIZE 8192  // Includes bounded syscall staging buffers
#define MAX_TASKS 32

// Task states
typedef enum {
    TASK_READY,       // Ready to run
    TASK_RUNNING,     // Currently executing
    TASK_BLOCKED,     // Waiting for an event
    TASK_TERMINATED   // Finished execution
} task_state_t;

// Process Control Block
typedef struct pcb {
    uint32_t pid;                    // Process ID
    task_state_t state;              // Current state
    uint32_t esp;                    // Saved stack pointer
    uint32_t kernel_stack;           // Base of allocated stack (for kfree)
    uint32_t user_stack;
    uint32_t kernel_stack_top;       // Top of stack (high address)
    uint32_t wake_tick;              // Deadline for timer sleep
    uint8_t sleeping;
    void (*entry_point)(void);       // Task entry function
    struct pcb *next;                // For scheduler linked list
} pcb_t;

// Initialize multitasking system
void task_init(void);

// Create a new kernel task
pcb_t* task_create(void (*entry)(void));

// Terminate current task
void task_exit(void) __attribute__((noreturn));
void task_reap(void);

// Get current running task
pcb_t* task_current(void);

// Yield CPU to next task (voluntary)
void task_yield(void);

/* The caller must protect its event check before blocking. The IRQ state on
 * return matches the state at entry; PID 0 may never block. */
void task_block_current(void);
void task_wake(pcb_t *task);
/* Sleep callers cap relative deadlines at INT32_MAX ticks for wrap safety. */
void task_sleep_ticks(uint32_t ticks);
void task_wake_due(uint32_t now);

#endif
