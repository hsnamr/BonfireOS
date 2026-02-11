#ifndef BONFIRE_KEYBOARD_H
#define BONFIRE_KEYBOARD_H

#include <kernel/types.h>

#define KEYBUF_SIZE 256

/* Non-blocking: get next character if available (0 = none). */
char keyboard_getchar(void);
/* Non-blocking: get next key event (scancode, down=1/0). Returns 0 if none. */
int keyboard_get_scancode(uint8_t *scancode, int *down);
/* Discard all pending scancode events. */
void keyboard_clear_scancodes(void);
/* Called from IRQ handler when scancode is received. */
void keyboard_irq_handler(void);

#endif /* BONFIRE_KEYBOARD_H */
