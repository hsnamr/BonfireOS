#include <kernel/keyboard.h>
#include <kernel/mouse.h>
#include <kernel/types.h>

void doom_input_get_key_impl(uint8_t *key, int *down)
{
    if (!keyboard_get_scancode(key, down)) *key = 0;
}

void doom_input_mouse_impl(int *dx, int *dy, int *buttons)
{
    mouse_poll(dx, dy, buttons);
}

void doom_input_clear_impl(void)
{
    keyboard_clear_scancodes();
    mouse_clear();
}
