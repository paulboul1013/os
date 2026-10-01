#include "timer.h"
#include "isr.h"
#include "ports.h"
#include "scheduler.h"
#include "task.h"
#include "../libc/function.h"

volatile uint32_t tick = 0;
volatile uint32_t timer_freq = 0;

static void timer_callback(registers_t *regs){
    tick++;
    task_wake_due(tick);

    // Call scheduler for preemptive multitasking
    scheduler_timer_handler(regs);
}

void init_timer(uint32_t freq) {
    timer_freq = freq;
    //install the function just wrote
    register_interrupt_handler(IRQ0,timer_callback);

    //Get the PIT value: hardware clock at 1193180 Hz
    uint32_t divisor = 1193180 / freq;
    uint8_t low=(uint8_t)(divisor & 0xFF);
    uint8_t high=(uint8_t)((divisor>>8) &0xFF);
    
    //send the command
    port_byte_out(0x43,0x34); //command port: Channel 0, Lobyte/Hibyte, Mode 2, Binary
    port_byte_out(0x40,low);
    port_byte_out(0x40,high);
}

int sleep(uint32_t seconds) {
    if (!seconds) return 0;
    if (!timer_freq || seconds > 0x7fffffff / timer_freq) return -1;
    task_sleep_ticks(seconds * timer_freq);
    return 0;
}

int sleep_ms(uint32_t ms) {
    if (!ms) return 0;
    if (!timer_freq) return -1;
    uint32_t whole = ms / 1000, remainder = ms % 1000;
    if (whole > 0x7fffffff / timer_freq ||
        (remainder && timer_freq > (0xffffffffu - 999) / remainder)) return -1;
    uint32_t fraction = (remainder * timer_freq + 999) / 1000;
    uint32_t ticks = whole * timer_freq;
    if (ticks > 0x7fffffff - fraction) return -1;
    ticks += fraction;
    task_sleep_ticks(ticks);
    return 0;
}

// Play sound using PIT Channel 2
void play_sound(uint32_t nFrequency) {
    uint32_t Div;
    uint8_t tmp;

    // Set PIT Channel 2 frequency
    Div = 1193180 / nFrequency;
    port_byte_out(0x43, 0xB6);
    port_byte_out(0x42, (uint8_t) (Div & 0xFF));
    port_byte_out(0x42, (uint8_t) ((Div >> 8) & 0xFF));

    // And play the sound using the PC speaker
    tmp = port_byte_in(0x61);
    if (tmp != (tmp | 3)) {
        port_byte_out(0x61, tmp | 3);
    }
}

// Stop sound
void nosound() {
    uint8_t tmp = port_byte_in(0x61) & 0xFC;
    port_byte_out(0x61, tmp);
}
