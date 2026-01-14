#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void init_timer(uint32_t freq);
void sleep(uint32_t seconds);
void sleep_ms(uint32_t ms);
void play_sound(uint32_t nFrequency);
void nosound();

#endif