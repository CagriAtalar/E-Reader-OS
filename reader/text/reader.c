/* reader.c — Text reader with LIST/READ state machine
 *
 * STATE_LIST: shows file names, w/s navigates, p opens
 * STATE_READ: shows file content with word wrap, a/d pages, q returns
 *
 * Screen layout (80x25):
 *   Rows 0-23: content (file list or text)
 *   Row 24:    hint bar
 */
#include "reader.h"
#include "../kernel/vga.h"
#include "../kernel/serial.h"
#include "../kernel/manifest.h"
#include "../kernel/libc_min.h"

#define CONTENT_ROWS 24       /* Rows 0-23 for content */
#define HINT_ROW     24       /* Row 24 for hints */

typedef enum {
    STATE_LIST,
    STATE_READ
} reader_state_t;

static reader_state_t state;
static int cursor;            /* Selected index in list */
static int current_file;      /* Index of opened file */
static int current_page;      /* Current page number (0-based) */

/* Page index: array of byte offsets where each screen page starts.
 * Pre-computed when a file is opened. */
#define MAX_PAGES 4096
static uint32_t page_offsets[MAX_PAGES];
static int total_pages;

/* ---- Word-wrap page computation ---- */

/* Compute all page start offsets for the given text.
 * A "page" is CONTENT_ROWS rows of 80-column word-wrapped text.
 * Returns total number of pages. */
static int compute_pages(const char *text, uint32_t size) {
    uint32_t pos = 0;
    int pages = 0;

    while (pos <= size && pages < MAX_PAGES) {
        page_offsets[pages++] = pos;
        if (pos >= size) break;

        /* Advance through CONTENT_ROWS lines */
        for (int row = 0; row < CONTENT_ROWS && pos < size; row++) {
            /* Find where this line ends */
            int col = 0;
            uint32_t line_start = pos;

            while (pos < size && col < VGA_WIDTH) {
                if (text[pos] == '\n') {
                    pos++;
                    break;
                }
                if (text[pos] == '\r') {
                    pos++;
                    if (pos < size && text[pos] == '\n') pos++;
                    break;
                }
                pos++;
                col++;
            }

            /* If we hit VGA_WIDTH without a newline, we wrapped.
             * Try to break at the last space for word wrap. */
            if (col >= VGA_WIDTH && pos < size && text[pos] != '\n' && text[pos] != '\r') {
                /* Look back for a space to break on */
                uint32_t scan = pos;
                while (scan > line_start && text[scan - 1] != ' ') {
                    scan--;
                }
                if (scan > line_start) {
                    pos = scan; /* Break after the space */
                }
                /* else: no space found, hard break at VGA_WIDTH */
            }
        }
    }

    return pages;
}

/* ---- Rendering ---- */

static void render_list(void) {
    vga_clear();

    /* Title */
    vga_print_at(0, 0, "=== cagOS Text Reader ===", VGA_COLOR_WHITE);

    /* List files starting at row 2 */
    int max_visible = CONTENT_ROWS - 2; /* Leave room for title + blank line */
    int start_idx = 0;

    /* Scroll the list if cursor is past visible area */
    if (cursor >= max_visible) {
        start_idx = cursor - max_visible + 1;
    }

    for (int i = 0; i < max_visible && (start_idx + i) < text_count; i++) {
        int idx = start_idx + i;
        int row = i + 2;
        uint8_t attr = (idx == cursor) ? VGA_COLOR_HIGHLIGHT : VGA_COLOR_WHITE;

        /* Clear row first */
        vga_clear_row(row, attr);

        /* Print "> " prefix for selected item */
        if (idx == cursor) {
            vga_print_at(0, row, "> ", attr);
            vga_print_at(2, row, text_manifest[idx].name, attr);
        } else {
            vga_print_at(2, row, text_manifest[idx].name, attr);
        }
    }

    /* Hint bar */
    vga_clear_row(HINT_ROW, VGA_COLOR_WHITE);
    vga_print_at(0, HINT_ROW, " [w] up  [s] down  [p] open", VGA_COLOR_WHITE);
}

/* Render a single page of text */
static void render_read(void) {
    vga_clear();

    const char *text = __text_blob_start + text_manifest[current_file].offset;
    uint32_t size = text_manifest[current_file].size;

    if (size == 0) {
        vga_print_at(0, 0, "(empty)", VGA_COLOR_WHITE);
        vga_clear_row(HINT_ROW, VGA_COLOR_WHITE);
        vga_print_at(0, HINT_ROW, " [q] back", VGA_COLOR_WHITE);
        return;
    }

    uint32_t pos = page_offsets[current_page];

    for (int row = 0; row < CONTENT_ROWS && pos < size; row++) {
        int col = 0;
        uint32_t line_start = pos;
        uint32_t line_end = pos;

        /* Measure the line (same logic as compute_pages) */
        uint32_t scan = pos;
        while (scan < size && col < VGA_WIDTH) {
            if (text[scan] == '\n') {
                line_end = scan;
                scan++;
                break;
            }
            if (text[scan] == '\r') {
                line_end = scan;
                scan++;
                if (scan < size && text[scan] == '\n') scan++;
                break;
            }
            line_end = scan + 1;
            scan++;
            col++;
        }

        uint32_t next_pos = scan;

        /* Word wrap: if we hit VGA_WIDTH, try to break at space */
        if (col >= VGA_WIDTH && scan < size && text[scan] != '\n' && text[scan] != '\r') {
            uint32_t bp = scan;
            while (bp > line_start && text[bp - 1] != ' ') {
                bp--;
            }
            if (bp > line_start) {
                line_end = bp;
                next_pos = bp;
            }
        }

        /* Draw the line */
        int draw_col = 0;
        for (uint32_t p = line_start; p < line_end && draw_col < VGA_WIDTH; p++) {
            char c = text[p];
            if (c == '\t') {
                /* Tab: advance to next 8-column boundary */
                int next_tab = (draw_col + 8) & ~7;
                while (draw_col < next_tab && draw_col < VGA_WIDTH) {
                    vga_putchar_at(draw_col, row, ' ', VGA_COLOR_WHITE);
                    draw_col++;
                }
            } else {
                vga_putchar_at(draw_col, row, c, VGA_COLOR_WHITE);
                draw_col++;
            }
        }

        pos = next_pos;
    }

    /* Hint bar with page info */
    vga_clear_row(HINT_ROW, VGA_COLOR_WHITE);

    /* Build hint string */
    char hint[80];
    int hi = 0;

    /* " [a] prev  [d] next  [q] back   Page X/Y" */
    const char *h = " [a] prev  [d] next  [q] back";
    while (*h && hi < 60) hint[hi++] = *h++;

    /* Page indicator on the right side */
    char pg[16], tot[16];
    itoa(current_page + 1, pg, 10);
    itoa(total_pages, tot, 10);

    /* Pad to column 60 */
    while (hi < 55) hint[hi++] = ' ';
    const char *pp = "Pg ";
    while (*pp) hint[hi++] = *pp++;
    char *s = pg;
    while (*s) hint[hi++] = *s++;
    hint[hi++] = '/';
    s = tot;
    while (*s) hint[hi++] = *s++;
    hint[hi] = '\0';

    vga_print_at(0, HINT_ROW, hint, VGA_COLOR_WHITE);
}

/* ---- Public API ---- */

void reader_init(void) {
    state = STATE_LIST;
    cursor = 0;
    current_file = 0;
    current_page = 0;
    total_pages = 0;
}

void reader_handle_key(char key) {
    if (key == 0) return;

    switch (state) {
    case STATE_LIST:
        switch (key) {
        case 'w':
            if (cursor > 0) cursor--;
            break;
        case 's':
            if (cursor < text_count - 1) cursor++;
            break;
        case 'p':
            if (text_count > 0) {
                current_file = cursor;
                current_page = 0;

                /* Compute pages for this file */
                const char *text = __text_blob_start + text_manifest[current_file].offset;
                uint32_t size = text_manifest[current_file].size;
                total_pages = compute_pages(text, size);
                if (total_pages == 0) total_pages = 1;

                serial_printf("selected: %s (%d bytes, %d pages)\n",
                              text_manifest[current_file].name,
                              size, total_pages);

                state = STATE_READ;
            }
            break;
        }
        break;

    case STATE_READ:
        switch (key) {
        case 'a':
            if (current_page > 0) current_page--;
            break;
        case 'd':
            if (current_page < total_pages - 1) current_page++;
            break;
        case 'q':
            state = STATE_LIST;
            break;
        }
        break;
    }
}

void reader_render(void) {
    switch (state) {
    case STATE_LIST:
        render_list();
        break;
    case STATE_READ:
        render_read();
        break;
    }
}
