#include "timer.h"
#include "isr.h"
#include "ports.h"
#include "../libc/function.h"

volatile uint32_t tick = 0;
volatile uint32_t timer_freq = 0;

static void timer_callback(registers_t *regs){
    tick++;
    UNUSED(regs);
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

void sleep(uint32_t seconds) {
    volatile uint32_t target_tick = tick + (seconds * timer_freq);
    asm volatile("sti"); // make sure interrupts are enabled
    while (tick < target_tick) {
        asm volatile("hlt");
    }
}

