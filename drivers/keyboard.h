#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void init_keyboard();

extern volatile int kbd_line_ready;
extern char kbd_line_buffer[256];

#endif
