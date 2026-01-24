#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define KERNEL_STACK_SIZE 4096  // 4KB stack per task
#define MAX_TASKS 32

// Task states
typedef enum {
    TASK_READY,       // Ready to run
    TASK_RUNNING,     // Currently executing
    TASK_BLOCKED,     // Waiting (future use)
    TASK_TERMINATED   // Finished execution
} task_state_t;

// Process Control Block
typedef struct pcb {
    uint32_t pid;                    // Process ID
    task_state_t state;              // Current state
    uint32_t esp;                    // Saved stack pointer
    uint32_t kernel_stack;           // Base of allocated stack (for kfree)
    uint32_t kernel_stack_top;       // Top of stack (high address)
    void (*entry_point)(void);       // Task entry function
    struct pcb *next;                // For scheduler linked list
} pcb_t;

// Initialize multitasking system
void task_init(void);

// Create a new kernel task
pcb_t* task_create(void (*entry)(void));

// Terminate current task
void task_exit(void);

// Get current running task
pcb_t* task_current(void);

// Yield CPU to next task (voluntary)
void task_yield(void);

#endif
