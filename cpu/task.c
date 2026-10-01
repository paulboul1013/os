#include "task.h"
#include "scheduler.h"
#include "paging.h"
#include "pmm.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"

static pcb_t task_pool[MAX_TASKS];
static uint32_t next_pid = 1;
extern volatile uint32_t tick;

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
    main_task->kernel_stack_top = 0x90000;
    main_task->esp = 0;               // Will be saved on first context switch
    main_task->entry_point = 0;
    main_task->next = 0;

    scheduler_add_task(main_task);
}

static void task_start(void) {
    task_reap();
    asm volatile("sti");
    task_current()->entry_point();
    task_exit();
}

pcb_t* task_create(void (*entry)(void)) {
    uint32_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    task_reap();
    // Find free slot in task pool
    pcb_t *task = 0;
    for (int i = 1; i < MAX_TASKS; i++) {  // Start at 1, slot 0 is main task
        if (task_pool[i].pid == 0) {
            task = &task_pool[i];
            break;
        }
    }
    if (!task) goto failed;

    // Kernel stacks stay supervisor-only; heap alignment is not required.
    uint32_t stack_base = kmalloc(KERNEL_STACK_SIZE, 0, 0);
    if (!stack_base) goto failed;

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
    // 3. ret (jump to trampoline)
    uint32_t *sp = (uint32_t*)stack_top;

    // Return address - where 'ret' will jump to
    *--sp = (uint32_t)task_start;

    // Saved registers (will be popped by context_switch)
    *--sp = 0;      // EBP
    *--sp = 0;      // EBX
    *--sp = 0;      // ESI
    *--sp = 0;      // EDI
    *--sp = 0x2;  // EFLAGS: IF=0 until trampoline finishes reaping

    task->esp = (uint32_t)sp;

    // Add to scheduler
    scheduler_add_task(task);

    asm volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
    return task;
failed:
    asm volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
    return 0;
}

/* Called only with IRQs disabled, after the dead task has been unlinked. */
void task_reap(void) {
    for (int i = 1; i < MAX_TASKS; i++) {
        pcb_t *task = &task_pool[i];
        if (task != task_current() && task->pid && task->state == TASK_TERMINATED) {
            if (task->user_stack) {
                paging_set_user_stack(task->user_stack, 0);
                memory_set((uint8_t*)task->user_stack, 0, PAGE_SIZE);
                pmm_free_frame(task->user_stack);
            }
            kfree((void*)task->kernel_stack);
            memory_set((uint8_t*)task, 0, sizeof(*task));
        }
    }
}

void task_exit(void) {
    asm volatile("cli" ::: "memory");
    pcb_t *current = scheduler_current();
    if (current && current->pid) {
        keyboard_cancel_waiter(current);
        current->sleeping = 0;
        current->state = TASK_TERMINATED;
        schedule();
    }
    for (;;) asm volatile("cli; hlt");
}

pcb_t* task_current(void) {
    return scheduler_current();
}

void task_yield(void) {
    schedule();
}

void task_block_current(void) {
    uint32_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    pcb_t *current = task_current();
    if (current && current->pid && current->state == TASK_RUNNING) {
        current->state = TASK_BLOCKED;
        schedule();
    }
    asm volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

void task_wake(pcb_t *task) {
    uint32_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    if (task && task->pid && task->state == TASK_BLOCKED) {
        task->sleeping = 0;
        task->state = TASK_READY;
    }
    asm volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

void task_sleep_ticks(uint32_t ticks) {
    if (!ticks) return;
    uint32_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    pcb_t *current = task_current();
    if (current && current->pid) {
        current->wake_tick = tick + ticks;
        current->sleeping = 1;
        task_block_current();
    }
    asm volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

void task_wake_due(uint32_t now) {
    uint32_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    for (int i = 1; i < MAX_TASKS; i++) {
        pcb_t *task = &task_pool[i];
        if (task->sleeping && task->state == TASK_BLOCKED &&
            (int32_t)(now - task->wake_tick) >= 0) task_wake(task);
    }
    asm volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}
