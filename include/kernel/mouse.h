#ifndef BONFIRE_MOUSE_H
#define BONFIRE_MOUSE_H

#include <kernel/types.h>

#define MOUSE_BTN_LEFT   1
#define MOUSE_BTN_RIGHT  2
#define MOUSE_BTN_MIDDLE 4

void mouse_init(void);
void mouse_poll(int *dx, int *dy, int *buttons);
void mouse_clear(void);

#endif /* BONFIRE_MOUSE_H */
