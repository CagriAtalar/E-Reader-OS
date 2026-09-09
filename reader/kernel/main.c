/* main.c — cagOS readerOS kernel entry point
 *
 * Called by the bootloader after switching to 32-bit protected mode.
 * The kernel is loaded at 0x100000 (1MB).
 */
#include "serial.h"
#include "vga.h"
#include "ps2.h"
#include "libc_min.h"
#include "manifest.h"
#include "../text/reader.h"

/* Kernel entry point — called from bootloader */
void kmain(void) {
    /* Initialize serial port for debug output */
    serial_init();
    serial_puts("stage2 ok\n");
    serial_printf("cagOS readerOS v0 booting...\n");

    /* Initialize VGA text mode */
    vga_init();
    serial_puts("VGA init ok\n");

    /* Initialize PS/2 keyboard */
    ps2_init();
    serial_puts("PS/2 init ok\n");

    /* Initialize the reader state machine */
    reader_init();
    serial_printf("Manifest: %d text files\n", text_count);

    /* Initial render */
    reader_render();

    /* Main event loop: poll keyboard, handle input, re-render */
    for (;;) {
        char key = ps2_poll();
        if (key) {
            reader_handle_key(key);
            reader_render();
        }

        /* Small delay to avoid burning CPU on polling */
        for (volatile int i = 0; i < 10000; i++);
    }
}
