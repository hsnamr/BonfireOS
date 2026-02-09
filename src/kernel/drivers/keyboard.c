/**
 * PS/2 keyboard driver (scancode set 1).
 * Uses a ring buffer; keyboard_irq_handler is called from IDT IRQ1.
 */

#include <kernel/keyboard.h>
#include <kernel/port.h>
#include <kernel/irq.h>

#define KEYB_DATA  0x60
#define KEYB_STATUS 0x64

static char keybuf[KEYBUF_SIZE];
static size_t keybuf_head;
static size_t keybuf_tail;
static bool keybuf_full;

/* US QWERTY scancode set 1 -> ASCII (make codes only; ignore break for simplicity) */
static const char scancode_to_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void keyboard_irq_handler(void)
{
    uint8_t sc = inb(KEYB_DATA);
    if (sc & 0x80) return; /* break code */
    if (sc >= 128) return;
    char c = scancode_to_ascii[sc];
    if (!c) return;
    if (keybuf_full) return;
    keybuf[keybuf_head] = c;
    keybuf_head = (keybuf_head + 1) % KEYBUF_SIZE;
    if (keybuf_head == keybuf_tail)
        keybuf_full = true;
}

char keyboard_getchar(void)
{
    if (!keybuf_full && keybuf_head == keybuf_tail)
        return 0;
    char c = keybuf[keybuf_tail];
    keybuf_tail = (keybuf_tail + 1) % KEYBUF_SIZE;
    keybuf_full = false;
    return c;
}
