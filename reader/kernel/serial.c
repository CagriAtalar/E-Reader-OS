/* serial.c — COM1 (0x3F8) serial port driver */
#include "serial.h"
#include "libc_min.h"

#define COM1_PORT 0x3F8

/* x86 I/O port helpers */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);  /* Disable interrupts */
    outb(COM1_PORT + 3, 0x80);  /* Enable DLAB */
    outb(COM1_PORT + 0, 0x01);  /* Divisor = 1 (115200 baud) */
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);  /* 8 bits, no parity, 1 stop bit */
    outb(COM1_PORT + 2, 0xC7);  /* Enable FIFO, clear, 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

static int serial_tx_ready(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_putc(char c) {
    while (!serial_tx_ready());
    outb(COM1_PORT, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

/* Minimal printf: supports %s, %d, %u, %x, %c, %% */
void serial_printf(const char *fmt, ...) {
    /* GCC variadic handling for -ffreestanding */
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            if (*fmt == '\n') serial_putc('\r');
            serial_putc(*fmt++);
            continue;
        }
        fmt++; /* skip '%' */

        switch (*fmt) {
        case 's': {
            const char *s = __builtin_va_arg(args, const char *);
            serial_puts(s ? s : "(null)");
            break;
        }
        case 'd': {
            int v = __builtin_va_arg(args, int);
            char buf[32];
            itoa(v, buf, 10);
            serial_puts(buf);
            break;
        }
        case 'u': {
            unsigned int v = __builtin_va_arg(args, unsigned int);
            char buf[32];
            itoa((int)v, buf, 10); /* good enough for v0 */
            serial_puts(buf);
            break;
        }
        case 'x': {
            unsigned int v = __builtin_va_arg(args, unsigned int);
            char buf[32];
            itoa((int)v, buf, 16);
            serial_puts(buf);
            break;
        }
        case 'c': {
            char c = (char)__builtin_va_arg(args, int);
            serial_putc(c);
            break;
        }
        case '%':
            serial_putc('%');
            break;
        default:
            serial_putc('%');
            serial_putc(*fmt);
            break;
        }
        fmt++;
    }

    __builtin_va_end(args);
}

void panic(const char *msg) {
    serial_puts("\n!!! PANIC: ");
    serial_puts(msg);
    serial_puts(" !!!\n");
    /* Halt */
    __asm__ volatile ("cli; hlt");
    for (;;);
}
