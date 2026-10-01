#include "scheduler.h"
#include "isr.h"
#include "../drivers/screen.h"
#include "../libc/function.h"
#include "tss.h"

static pcb_t *current_task = 0;
static pcb_t *ready_queue_head = 0;
static pcb_t *ready_queue_tail = 0;
static uint32_t task_count = 0;
static int scheduler_enabled = 0;

void scheduler_init(void) {
    current_task = 0;
    ready_queue_head = 0;
    ready_queue_tail = 0;
    task_count = 0;
    scheduler_enabled = 0;
}

void scheduler_add_task(pcb_t *task) {
    task->next = 0;

    if (!ready_queue_head) {
        // First task
        ready_queue_head = task;
        ready_queue_tail = task;
        current_task = task;
    } else {
        // Add to end of queue
        ready_queue_tail->next = task;
        ready_queue_tail = task;
    }

    task_count++;
}

pcb_t* scheduler_current(void) {
    return current_task;
}

void scheduler_enable(void) {
    scheduler_enabled = 1;
}

// Round-robin scheduler
void schedule(void) {
    uint32_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    task_reap();
    if (!scheduler_enabled) goto done;
    if (task_count <= 1) goto done;
    if (!current_task) goto done;

    pcb_t *old_task = current_task;
    pcb_t *next_task = 0;

    // Find next ready task (round-robin)
    pcb_t *candidate = old_task->next;
    if (!candidate) {
        candidate = ready_queue_head;  // Wrap around
    }

    // Search for a READY task
    pcb_t *start = candidate;
    do {
        if (candidate->state == TASK_READY) {
            next_task = candidate;
            break;
        }
        // Also allow current task if it's still RUNNING
        if (candidate == old_task && candidate->state == TASK_RUNNING) {
            next_task = candidate;
            break;
        }
        candidate = candidate->next;
        if (!candidate) {
            candidate = ready_queue_head;
        }
    } while (candidate != start);

    // No other task to switch to
    if (!next_task || next_task == old_task) goto done;

    if (old_task->state == TASK_TERMINATED) {
        pcb_t *previous = 0;
        for (pcb_t *p = ready_queue_head; p != old_task; p = p->next) previous = p;
        if (previous) previous->next = old_task->next;
        else ready_queue_head = old_task->next;
        if (ready_queue_tail == old_task) ready_queue_tail = previous;
        old_task->next = 0;
        task_count--;
    }

    // Update states
    if (old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    next_task->state = TASK_RUNNING;
    current_task = next_task;

    // 確保回到 Kernel 時有獨立的 Kernel Stack，不更新的話新 Task 會蓋壞舊 Task 的 Stack
    if (next_task->kernel_stack_top != 0) {
        tss_set_kernel_stack(next_task->kernel_stack_top);
    }
#ifdef SCHED_TEST
    extern void scheduling_switch_observed(pcb_t*, pcb_t*);
    scheduling_switch_observed(old_task, next_task);
#endif

    // Perform context switch
    context_switch(&old_task->esp, next_task->esp);
    task_reap();
done:
    asm volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

// Timer interrupt handler for preemptive scheduling
void scheduler_timer_handler(registers_t *regs) {
    /* IRQ stub keeps the complete frame on this task's kernel stack. Ring 3
     * frames also contain SS:ESP; iret consumes those only on privilege exit. */
#ifdef SCHED_TEST
    extern void scheduling_timer_observed(registers_t*);
    scheduling_timer_observed(regs);
#else
    (void)regs;
#endif
    schedule();
}
