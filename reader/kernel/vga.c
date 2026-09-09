/* vga.c — VGA text mode (80x25) driver, writes directly to 0xB8000 */
#include "vga.h"

static volatile uint16_t *vga_buffer = (volatile uint16_t *)VGA_ADDR;

static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

void vga_init(void) {
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', VGA_COLOR_WHITE);
    }
}

void vga_putchar_at(int col, int row, char c, uint8_t attr) {
    if (col < 0 || col >= VGA_WIDTH || row < 0 || row >= VGA_HEIGHT) return;
    /* ASCII-safe: if outside printable range 32-126, show '?' */
    if (c < 32 || c > 126) c = '?';
    vga_buffer[row * VGA_WIDTH + col] = vga_entry(c, attr);
}

void vga_print_at(int col, int row, const char *s, uint8_t attr) {
    while (*s && col < VGA_WIDTH) {
        vga_putchar_at(col, row, *s, attr);
        col++;
        s++;
    }
}

void vga_set_row_attr(int row, uint8_t attr) {
    if (row < 0 || row >= VGA_HEIGHT) return;
    for (int col = 0; col < VGA_WIDTH; col++) {
        uint16_t entry = vga_buffer[row * VGA_WIDTH + col];
        char c = (char)(entry & 0xFF);
        vga_buffer[row * VGA_WIDTH + col] = vga_entry(c, attr);
    }
}

void vga_clear_row(int row, uint8_t attr) {
    if (row < 0 || row >= VGA_HEIGHT) return;
    for (int col = 0; col < VGA_WIDTH; col++) {
        vga_buffer[row * VGA_WIDTH + col] = vga_entry(' ', attr);
    }
}
