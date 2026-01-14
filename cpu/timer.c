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
    sleep_ms(seconds * 1000);
}

void sleep_ms(uint32_t ms) {
    uint32_t ticks_to_wait = (ms * timer_freq) / 1000;
    if (ticks_to_wait == 0 && ms > 0) ticks_to_wait = 1;
    
    uint32_t target_tick = tick + ticks_to_wait;
    asm volatile("sti");
    while (tick < target_tick) {
        asm volatile("hlt");
    }
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

