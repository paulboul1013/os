#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"
#include "isr.h"

// Initialize the scheduler
void scheduler_init(void);

// Add a task to the ready queue
void scheduler_add_task(pcb_t *task);

// Get current running task
pcb_t* scheduler_current(void);

// Perform scheduling (called from timer IRQ or voluntarily)
void schedule(void);

// Enable/disable preemptive scheduling
void scheduler_enable(void);
void scheduler_disable(void);

// Timer interrupt handler for preemptive scheduling
void scheduler_timer_handler(registers_t *regs);

// Context switch assembly function
// Saves current ESP to *old_esp, loads new_esp as current ESP
extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

#endif
