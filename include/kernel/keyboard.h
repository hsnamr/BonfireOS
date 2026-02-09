#ifndef BONFIRE_KEYBOARD_H
#define BONFIRE_KEYBOARD_H

#include <kernel/types.h>

#define KEYBUF_SIZE 256

/* Non-blocking: get next character if available (0 = none). */
char keyboard_getchar(void);
/* Called from IRQ handler when scancode is received. */
void keyboard_irq_handler(void);

#endif /* BONFIRE_KEYBOARD_H */
