#include "stdio.h"
#include "string.h"
#include "../drivers/screen.h"

/* 
 * Internal formatting engine
 * Takes a character emitter function pointer and an arbitrary argument (state).
 */
typedef void (*emitter_t)(char c, void *arg);

static void itoa(int n, char str[], int base) {
    int i, sign;
    if ((sign = n) < 0 && base == 10) n = -n;
    i = 0;
    do {
        int r = (unsigned int)n % base;
        str[i++] = (r > 9) ? (r - 10 + 'a') : (r + '0');
    } while ((n = (unsigned int)n / base) > 0);

    if (sign < 0 && base == 10) str[i++] = '-';
    str[i] = '\0';
    reverse(str);
}

static void utoa(unsigned int n, char str[], int base) {
    int i = 0;
    do {
        int r = n % base;
        str[i++] = (r > 9) ? (r - 10 + 'a') : (r + '0');
    } while ((n /= base) > 0);
    str[i] = '\0';
    reverse(str);
}

/* Base worker function for formatting */
static int __vformat(emitter_t emit, void *arg, const char *format, va_list ap) {
    int written = 0;
    const char *p;
    char buffer[32];
    char *b;

    for (p = format; *p != '\0'; p++) {
        if (*p != '%') {
            emit(*p, arg);
            written++;
            continue;
        }

        p++; // skip '%'
        switch (*p) {
            case 'c': {
                char c = (char)va_arg(ap, int);
                emit(c, arg);
                written++;
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (s == 0) s = "(null)";
                while (*s) {
                    emit(*s++, arg);
                    written++;
                }
                break;
            }
            case 'd':
            case 'i': {
                int d = va_arg(ap, int);
                itoa(d, buffer, 10);
                for (b = buffer; *b; b++) {
                    emit(*b, arg);
                    written++;
                }
                break;
            }
            case 'u': {
                unsigned int u = va_arg(ap, unsigned int);
                utoa(u, buffer, 10);
                for (b = buffer; *b; b++) {
                    emit(*b, arg);
                    written++;
                }
                break;
            }
            case 'x':
            case 'p': {
                unsigned int x = va_arg(ap, unsigned int);
                utoa(x, buffer, 16);
                for (b = buffer; *b; b++) {
                    emit(*b, arg);
                    written++;
                }
                break;
            }
            case '%':
                emit('%', arg);
                written++;
                break;
            default:
                emit(*p, arg);
                written++;
                break;
        }
    }
    return written;
}

/* Emitters */

// Screen emitter
static void screen_emit(char c, void *arg) {
    (void)arg;
    kputchar(c);
}

// Buffer emitter (with size limit)
typedef struct {
    char *p;
    int size;
    int count;
} buf_state_t;

static void buffer_emit(char c, void *arg) {
    buf_state_t *state = (buf_state_t *)arg;
    if (state->count < state->size - 1) {
        *(state->p)++ = c;
    }
    state->count++;
}

/* 
 * The printf() family members and their hierarchy 
 * Based on the requested "Wraps over" structure.
 */

/* 1. Worker for screen output (acting as vfprintf to stdout) */
int vfprintf(FILE *stream, const char *format, va_list ap) {
    // For now, ignoring stream and always printing to screen
    (void)stream;
    return __vformat(screen_emit, 0, format, ap);
}

/* 2. vprintf wraps vfprintf */
int vprintf(const char *format, va_list ap) {
    return vfprintf((FILE*)stdout, format, ap);
}

/* 3. printf wraps vprintf */
int printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vprintf(format, ap);
    va_end(ap);
    return res;
}

/* 4. fprintf wraps vfprintf */
int fprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vfprintf(stream, format, ap);
    va_end(ap);
    return res;
}

/* 5. Worker for buffer formatting (vsnprintf) */
int vsnprintf(char *str, int size, const char *format, va_list ap) {
    buf_state_t state = {str, size, 0};
    int res = __vformat(buffer_emit, &state, format, ap);
    if (size > 0) {
        if (state.count < size) {
            *state.p = '\0';
        } else {
            str[size - 1] = '\0';
        }
    }
    return res;
}

/* 6. vsprintf wraps vsnprintf with a "magical number" (large limit) */
int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, 1024 * 1024, format, ap); // 1MB limit as "magical number"
}

/* 7. sprintf wraps vsprintf */
int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vsprintf(str, format, ap);
    va_end(ap);
    return res;
}

/* 8. snprintf wraps vsnprintf */
int snprintf(char *str, int size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vsnprintf(str, size, format, ap);
    va_end(ap);
    return res;
}
