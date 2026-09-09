/* libc_min.h — Minimal C runtime stubs */
#ifndef LIBC_MIN_H
#define LIBC_MIN_H

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef int            int32_t;
typedef short          int16_t;
typedef signed char    int8_t;
typedef unsigned int   size_t;
typedef int            ssize_t;

#define NULL ((void *)0)

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);

/* Simple integer to ASCII (decimal) */
void itoa(int value, char *buf, int base);

#endif /* LIBC_MIN_H */
