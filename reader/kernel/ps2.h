/* ps2.h — PS/2 keyboard (i8042), scancode set 1, polling */
#ifndef PS2_H
#define PS2_H

/* Returns ASCII char for recognized keys, 0 otherwise.
 * Only w, s, a, d, p, q are recognized. */
char ps2_poll(void);

void ps2_init(void);

#endif /* PS2_H */
