#ifndef USERCOPY_H
#define USERCOPY_H
#include <stdint.h>
#define USER_EFAULT (-14)
#define USER_ENAMETOOLONG (-36)
#define USER_STRING_MAX 1024
int user_range_valid(const void *ptr, uint32_t len, int write);
int copy_from_user(void *dst, const void *src, uint32_t len);
int copy_to_user(void *dst, const void *src, uint32_t len);
int copy_string_from_user(char *dst, const char *src, uint32_t capacity);
#endif
