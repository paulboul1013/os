#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

void enter_usermode(void *user_entry,uint32_t user_esp);
void lauch_user_task(void (*entry)(void));


#endif