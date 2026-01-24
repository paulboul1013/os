#include "scheduler.h"
#include "isr.h"
#include "../drivers/screen.h"
#include "../libc/function.h"

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

void scheduler_disable(void) {
    scheduler_enabled = 0;
}

// Round-robin scheduler
void schedule(void) {
    if (!scheduler_enabled) return;
    if (task_count <= 1) return;
    if (!current_task) return;

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
    if (!next_task || next_task == old_task) return;

    // Update states
    if (old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    next_task->state = TASK_RUNNING;
    current_task = next_task;

    // Perform context switch
    context_switch(&old_task->esp, next_task->esp);
}

// Timer interrupt handler for preemptive scheduling
void scheduler_timer_handler(registers_t *regs) {
    UNUSED(regs);
    schedule();
}
