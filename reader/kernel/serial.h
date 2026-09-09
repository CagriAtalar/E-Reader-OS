/* serial.h — COM1 serial port interface */
#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_printf(const char *fmt, ...);

/* Panic: dump message to serial and halt */
void panic(const char *msg);

#endif /* SERIAL_H */
