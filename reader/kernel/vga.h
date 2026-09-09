/* vga.h — VGA text mode (80x25) interface */
#ifndef VGA_H
#define VGA_H

#include "libc_min.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_ADDR   0xB8000

/* Colors (CGA text mode attribute byte) */
#define VGA_COLOR_BLACK  0x00
#define VGA_COLOR_WHITE  0x0F
#define VGA_COLOR_HIGHLIGHT 0x70  /* Black on white (inverted) */

void vga_init(void);
void vga_clear(void);
void vga_putchar_at(int col, int row, char c, uint8_t attr);
void vga_print_at(int col, int row, const char *s, uint8_t attr);
void vga_set_row_attr(int row, uint8_t attr);
void vga_clear_row(int row, uint8_t attr);

#endif /* VGA_H */
