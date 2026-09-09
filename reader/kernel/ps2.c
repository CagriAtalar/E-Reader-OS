/* ps2.c — PS/2 keyboard (i8042), scancode set 1, polling mode */
#include "ps2.h"
#include "libc_min.h"

#define PS2_DATA_PORT   0x60
#define PS2_STATUS_PORT 0x64

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void ps2_init(void) {
    /* Flush the i8042 output buffer */
    while (inb(PS2_STATUS_PORT) & 0x01) {
        inb(PS2_DATA_PORT);
    }
}

char ps2_poll(void) {
    /* Check if there's data in the output buffer */
    if (!(inb(PS2_STATUS_PORT) & 0x01)) {
        return 0;
    }

    uint8_t scancode = inb(PS2_DATA_PORT);

    /* Ignore key releases (bit 7 set) */
    if (scancode & 0x80) {
        return 0;
    }

    /* Scancode set 1 make codes for our keys */
    switch (scancode) {
    case 0x11: return 'w';
    case 0x1F: return 's';
    case 0x1E: return 'a';
    case 0x20: return 'd';
    case 0x19: return 'p';
    case 0x10: return 'q';
    default:   return 0;   /* Ignore everything else */
    }
}
