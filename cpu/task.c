#include "task.h"
#include "scheduler.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"

static pcb_t task_pool[MAX_TASKS];
static uint32_t next_pid = 1;

void task_init(void) {
    // Clear task pool
    memory_set((uint8_t*)task_pool, 0, sizeof(task_pool));

    // Initialize scheduler
    scheduler_init();

    // Create the "main" task representing current kernel execution
    // This task uses the boot stack, not an allocated one
    pcb_t *main_task = &task_pool[0];
    main_task->pid = 0;
    main_task->state = TASK_RUNNING;
    main_task->kernel_stack = 0;      // Uses boot stack, don't free
    main_task->kernel_stack_top = 0;
    main_task->esp = 0;               // Will be saved on first context switch
    main_task->entry_point = 0;
    main_task->next = 0;

    scheduler_add_task(main_task);
}

pcb_t* task_create(void (*entry)(void)) {
    // Find free slot in task pool
    pcb_t *task = 0;
    for (int i = 1; i < MAX_TASKS; i++) {  // Start at 1, slot 0 is main task
        if (task_pool[i].state == TASK_TERMINATED || task_pool[i].pid == 0) {
            task = &task_pool[i];
            break;
        }
    }
    if (!task) return 0;

    // Allocate kernel stack (4KB, page-aligned)
    uint32_t stack_base = kmalloc(KERNEL_STACK_SIZE, 1, 0);
    if (!stack_base) return 0;

    uint32_t stack_top = stack_base + KERNEL_STACK_SIZE;

    // Initialize PCB
    task->pid = next_pid++;
    task->state = TASK_READY;
    task->kernel_stack = stack_base;
    task->kernel_stack_top = stack_top;
    task->entry_point = entry;
    task->next = 0;

    // Set up initial stack frame for context_switch
    // When context_switch restores this task, it will:
    // 1. popf (restore EFLAGS)
    // 2. pop edi, esi, ebx, ebp
    // 3. ret (jump to entry point)
    uint32_t *sp = (uint32_t*)stack_top;

    // Return address - where 'ret' will jump to
    *--sp = (uint32_t)entry;

    // Saved registers (will be popped by context_switch)
    *--sp = 0;      // EBP
    *--sp = 0;      // EBX
    *--sp = 0;      // ESI
    *--sp = 0;      // EDI
    *--sp = 0x202;  // EFLAGS: IF=1 (interrupts enabled), reserved bit 1 set

    task->esp = (uint32_t)sp;

    // Add to scheduler
    scheduler_add_task(task);

    return task;
}

void task_exit(void) {
    pcb_t *current = scheduler_current();
    if (!current) return;

    current->state = TASK_TERMINATED;

    // Free stack if it was allocated (not the main task)
    if (current->kernel_stack) {
        kfree((void*)current->kernel_stack);
        current->kernel_stack = 0;
    }

    // Force schedule to next task
    schedule();

    // Should never reach here
    while(1) {
        asm volatile("hlt");
    }
}

pcb_t* task_current(void) {
    return scheduler_current();
}

void task_yield(void) {
    schedule();
}
