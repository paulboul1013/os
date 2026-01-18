#include <stdint.h>

typedef void (*constructor)();

extern constructor __init_array_start;
extern constructor __init_array_end;
extern constructor __ctors_start;
extern constructor __ctors_end;

void call_global_constructors() {
    constructor* i;

    // Call init_array
    for (i = &__init_array_start; i < &__init_array_end; i++) {
        if (*i) (*i)();
    }
    
    // Call ctors (older systems)
    for (i = &__ctors_start; i < &__ctors_end; i++) {
        if (*i) (*i)();
    }
}
