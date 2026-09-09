/* linker.ld — Kernel linker script
 *
 * Places kernel at 0x100000 (1MB).
 * The text blob is appended by the build system after the kernel binary,
 * so __text_blob_start is computed at build time and patched.
 */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(kmain)

SECTIONS
{
    . = 0x100000;

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }

    __kernel_end = .;

    /* Text blob is placed right after the kernel in the disk image.
     * The build system calculates the actual memory address and
     * provides __text_blob_start via a generated object file. */

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
