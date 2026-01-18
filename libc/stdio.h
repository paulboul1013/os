#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

/* The printf family */
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, int size, const char *format, ...);

/* Variadic versions */
int vprintf(const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, int size, const char *format, va_list ap);

/* 
 * Dummy versions for completeness (mapped to screen output)
 * In a real OS, these would use a FILE stream.
 */
typedef int FILE;
#define stdout 1
#define stderr 2
int fprintf(FILE *stream, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);

#endif
