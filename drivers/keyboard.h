#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "../cpu/task.h"

void init_keyboard();
void keyboard_prepare_input_line();

extern volatile int kbd_line_ready;
/* One pending line and one reader. Returns byte count or -16 when busy. */
int keyboard_read_line(char *buffer, uint32_t capacity);
void keyboard_cancel_waiter(pcb_t *task);

#endif
