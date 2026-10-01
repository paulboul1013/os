#ifdef SCHED_TEST
#include "../cpu/task.h"
#include "../cpu/ports.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../cpu/timer.h"
#include "../cpu/usermode.h"
#include "../cpu/paging.h"
#include "../cpu/tss.h"

static volatile unsigned steps_a, steps_b;
static volatile unsigned resumed;
static pcb_t *blocked_task;
static pcb_t *reader_task;
static volatile int read_result = -99;
static char read_text[256];
static volatile uint32_t short_start, short_end, long_start, long_end;
static volatile unsigned sleep_progress;
static volatile int short_done, long_done;
static volatile uint32_t background_ticks;
static volatile int shell_finished;
static volatile uint32_t sleep_probe_progress;
volatile int scheduling_hold_before_block;
pcb_t *scheduling_exit_reader;
volatile unsigned scheduling_enter_count;
static volatile int pending_result = -99;
static volatile int release_slot;
static int observe_spin;
static unsigned user_to_user, user_to_idle, idle_to_user, user_irqs;
extern tss_entry_t tss_entry;
extern volatile uint32_t tick;
extern volatile uint32_t spin_a, spin_b, spin_stop, spin_bad, spin_pid_a, spin_pid_b;
extern void user_spin_a(void), user_spin_b(void);
extern volatile uint32_t sleep_user_stage, sleep_user_bad;
extern void user_sleep_probe(void);
extern volatile uint32_t read_user_stage;
extern volatile int read_user_result;
extern char read_target[];
extern void user_read_probe(void);
extern void user_shell_main(void);

static void require(int condition) {
    if (condition) return;
    kprint("SCHED TEST FAILED\n");
    port_byte_out(0xf4, 0x11);
    for (;;) asm volatile("cli; hlt");
}

void scheduling_switch_observed(pcb_t *old, pcb_t *next) {
    require(tss_entry.esp0 == next->kernel_stack_top);
    if (!observe_spin) return;
    if (old->user_stack && next->user_stack) user_to_user++;
    if (old->user_stack && next->pid == 0) user_to_idle++;
    if (old->pid == 0 && next->user_stack) idle_to_user++;
}

void scheduling_timer_observed(registers_t *regs) {
    if (!observe_spin || (regs->cs & 3) != 3) return;
    pcb_t *current = task_current();
    uint32_t frame = (uint32_t)regs;
    require(frame >= current->kernel_stack && frame + sizeof(*regs) <= current->kernel_stack_top);
    require(tss_entry.esp0 == current->kernel_stack_top);
    require(regs->ss == 0x2b && regs->esp >= current->user_stack && regs->esp <= current->user_stack + 4096);
    require((regs->ds & 0xffff) == 0x2b && (regs->eflags & 0x200));
    user_irqs++;
}

static void worker_a(void) {
    for (unsigned i = 0; i < 20; i++) {
        steps_a++;
        task_yield();
    }
}

static void worker_b(void) {
    for (unsigned i = 0; i < 20; i++) {
        steps_b++;
        task_yield();
    }
}

static void blocker(void) {
    for (unsigned i = 0; i < 3; i++) {
        asm volatile("cli" ::: "memory");
        task_block_current();
        unsigned flags;
        asm volatile("pushf; pop %0" : "=r"(flags));
        require(!(flags & 0x200));
        resumed++;
        asm volatile("sti" ::: "memory");
    }
}

static void waker(void) {
    for (unsigned i = 0; i < 3; i++) {
        while (blocked_task->state != TASK_BLOCKED) task_yield();
        task_wake(blocked_task);
        require(blocked_task->state == TASK_READY);
        task_wake(blocked_task);
        require(blocked_task->state == TASK_READY);
        while (resumed == i) task_yield();
    }
}

static void reader(void) {
    read_result = keyboard_read_line(read_text, sizeof(read_text));
}

static void pending_reader(void) {
    pending_result = keyboard_read_line(read_text, sizeof(read_text));
}

static void cancelled_reader(void) {
    keyboard_read_line(read_text, sizeof(read_text));
    require(0);
}

static void hold_slot(void) {
    while (!release_slot) task_yield();
}

static void run_spin_a(void) { lauch_user_task(user_spin_a); }
static void run_spin_b(void) { lauch_user_task(user_spin_b); }
static void run_sleep_probe(void) { lauch_user_task(user_sleep_probe); }
static void run_read_probe(void) { lauch_user_task(user_read_probe); }

static void short_sleeper(void) {
    short_start = tick;
    sleep_ms(60);
    short_end = tick;
    require(!long_done);
    short_done = 1;
}

static void long_sleeper(void) {
    long_start = tick;
    sleep_ms(140);
    long_end = tick;
    require(short_done);
    long_done = 1;
}

static void sleep_worker(void) {
    while (!short_done || !long_done) {
        sleep_progress++;
        task_yield();
    }
}

static void shell_task(void) { lauch_user_task(user_shell_main); }

static void sleep_probe_worker(void) {
    while (sleep_user_stage != 2) {
        sleep_probe_progress++;
        task_yield();
    }
}

static void background_task(void) {
    uint32_t last = tick;
    while (!shell_finished) {
        if (tick != last) {
            background_ticks++;
            last = tick;
        }
        task_yield();
    }
}

void scheduling_tests(void) {
    require(task_create(worker_a) != 0);
    require(task_create(worker_b) != 0);
    while (steps_a < 20 || steps_b < 20) task_yield();
    require(steps_a == 20 && steps_b == 20);
    blocked_task = task_create(blocker);
    require(blocked_task != 0);
    require(task_create(waker) != 0);
    while (resumed < 3) task_yield();
    reader_task = task_create(reader);
    require(reader_task != 0);
    while (reader_task->state != TASK_BLOCKED) task_yield();
    require(keyboard_read_line(read_text, sizeof(read_text)) == -16);
    uint32_t waiting_tick = tick;
    kprint("SCHED READ WAIT\n");
    while (read_result == -99) task_yield();
    require((uint32_t)(tick - waiting_tick) >= 2);
    require(read_result == 4 && read_text[0] == 'w' && read_text[3] == 'e');
    kprint("SCHED EARLY INPUT\n");
    while (!kbd_line_ready) task_yield();
    require(keyboard_read_line(read_text, sizeof(read_text)) == 5);
    require(read_text[0] == 'e' && read_text[4] == 'y');
    unsigned enter_target = scheduling_enter_count + 2;
    kprint("SCHED SLOT FULL INPUT\n");
    while (scheduling_enter_count != enter_target) task_yield();
    require(kbd_line_ready);
    require(keyboard_read_line(read_text, sizeof(read_text)) == 5);
    require(read_text[0] == 'f' && read_text[4] == 't');
    require(!kbd_line_ready);
    kprint("SCHED EMPTY INPUT\n");
    while (!kbd_line_ready) task_yield();
    require(keyboard_read_line(read_text, sizeof(read_text)) == 0);
    require(read_text[0] == 0);
    uint32_t drain = tick;
    while ((uint32_t)(tick - drain) < 3) task_yield();
    pcb_t *cancelled = task_create(cancelled_reader);
    require(cancelled != 0);
    uint32_t cancelled_pid = cancelled->pid;
    while (cancelled->state != TASK_BLOCKED) task_yield();
    scheduling_exit_reader = cancelled;
    task_wake(cancelled);
    while (cancelled->pid == cancelled_pid && cancelled->state != TASK_TERMINATED) task_yield();
    scheduling_exit_reader = 0;
    pcb_t *reused = task_create(hold_slot);
    require(reused == cancelled && reused->pid != cancelled_pid);
    scheduling_hold_before_block = 1;
    pcb_t *different_reader = task_create(pending_reader);
    require(different_reader && different_reader != cancelled);
    while (pending_result == -99) task_yield();
    require(pending_result == 0 && !kbd_line_ready);
    release_slot = 1;
    pcb_t *user_reader = task_create(run_read_probe);
    require(user_reader != 0);
    while (read_user_stage != 1 || user_reader->state != TASK_BLOCKED) task_yield();
    paging_set_user_stack((uint32_t)read_target, 0);
    require(!paging_user_access((uint32_t)read_target, 1));
    kprint("SCHED COPY FAILURE INPUT\n");
    while (read_user_stage != 2) task_yield();
    paging_set_user_stack((uint32_t)read_target, 1);
    require(read_user_result == -14);
    pcb_t *spin_task_a = task_create(run_spin_a);
    pcb_t *spin_task_b = task_create(run_spin_b);
    require(spin_task_a && spin_task_b);
    observe_spin = 1;
    for (unsigned window = 0; window < 4; window++) {
        uint32_t before_a = spin_a, before_b = spin_b;
        require(sleep_ms(160) == 0);
        require(spin_a > before_a && spin_b > before_b && !spin_bad);
    }
    require(user_irqs >= 8 && user_to_user >= 4 && user_to_idle >= 4 && idle_to_user >= 4);
    observe_spin = 0;
    spin_stop = 1;
    uint32_t expected_pid_a = spin_task_a->pid, expected_pid_b = spin_task_b->pid;
    while ((spin_task_a->pid == expected_pid_a && spin_task_a->state != TASK_TERMINATED) ||
           (spin_task_b->pid == expected_pid_b && spin_task_b->state != TASK_TERMINATED)) task_yield();
    require(!spin_bad && spin_pid_a == expected_pid_a && spin_pid_b == expected_pid_b);
    asm volatile("cli" ::: "memory");
    tick = 0xfffffffd;
    asm volatile("sti" ::: "memory");
    pcb_t *short_task = task_create(short_sleeper);
    pcb_t *long_task = task_create(long_sleeper);
    require(short_task && long_task && task_create(sleep_worker));
    while (short_task->state != TASK_BLOCKED || long_task->state != TASK_BLOCKED) task_yield();
    while (!short_done || !long_done) task_yield();
    require((uint32_t)(short_end - short_start) >= 3 && (uint32_t)(short_end - short_start) <= 5);
    require((uint32_t)(long_end - long_start) >= 7 && (uint32_t)(long_end - long_start) <= 9);
    require(short_end < short_start && long_end < long_start);
    require(sleep_progress > 0);
    require(sleep(0) == 0 && sleep(0xffffffff) == -1);
    pcb_t *user_sleeper = task_create(run_sleep_probe);
    require(user_sleeper && task_create(sleep_probe_worker));
    while (sleep_user_stage != 1 || user_sleeper->state != TASK_BLOCKED) task_yield();
    uint32_t sleep_seen = tick, progress_seen = sleep_probe_progress;
    while (sleep_user_stage != 2) task_yield();
    require((uint32_t)(tick - sleep_seen) >= 49);
    require(sleep_probe_progress > progress_seen && !sleep_user_bad);
    spin_stop = 0;
    spin_a = 0;
    pcb_t *shell = task_create(shell_task);
    pcb_t *compute = task_create(run_spin_a);
    require(shell && compute && task_create(background_task));
    uint32_t shell_pid = shell->pid;
    while (shell->state != TASK_BLOCKED) task_yield();
    uint32_t before = background_ticks, then = tick;
    while ((uint32_t)(tick - then) < 5) task_yield();
    require(background_ticks >= before + 3);
    kprint("SCHED SHELL BACKGROUND PASS\n");
    while (shell->pid == shell_pid && shell->state != TASK_TERMINATED) task_yield();
    shell_finished = 1;
    require(spin_a > 0 && !spin_bad);
    spin_stop = 1;
    uint32_t compute_pid = compute->pid;
    while (compute->pid == compute_pid && compute->state != TASK_TERMINATED) task_yield();
    require(spin_pid_a == compute_pid);
    kprint("SCHED TESTS PASS: block/wake, keyboard IRQ, Ring 3, sleep, shell\n");
    port_byte_out(0xf4, 0x10);
    for (;;) asm volatile("cli; hlt");
}
#endif
