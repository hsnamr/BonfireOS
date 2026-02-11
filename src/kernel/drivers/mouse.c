/**
 * PS/2 mouse driver (3-byte packets: flags, dx, dy).
 * Uses AUX port (data 0x60 when from mouse, command 0x64).
 */

#include <kernel/mouse.h>
#include <kernel/port.h>
#include <kernel/types.h>

#define DATA    0x60
#define STATUS  0x64
#define CMD     0x64

#define CMD_AUX_DISABLE  0xA7
#define CMD_AUX_ENABLE   0xA8
#define CMD_READ_CFG     0x20
#define CMD_WRITE_CFG    0x60
#define CMD_AUX_WRITE    0xD4

#define CFG_AUX_IRQ  0x02
#define CFG_AUX_CLK  0x20

static int mouse_dx, mouse_dy, mouse_buttons;
static uint8_t mouse_buf[3];
static int mouse_phase;
static bool mouse_ok;

static void mouse_wait_out(void)
{
    for (int i = 0; i < 100000; i++)
        if ((inb(STATUS) & 0x02) == 0) return;
}

static void mouse_wait_in(void)
{
    for (int i = 0; i < 100000; i++)
        if ((inb(STATUS) & 0x01)) return;
}

static void mouse_write(uint8_t val)
{
    mouse_wait_out();
    outb(CMD, CMD_AUX_WRITE);
    mouse_wait_out();
    outb(DATA, val);
}

static uint8_t mouse_read(void)
{
    mouse_wait_in();
    return inb(DATA);
}

void mouse_init(void)
{
    mouse_ok = false;
    mouse_phase = 0;
    mouse_dx = mouse_dy = mouse_buttons = 0;
    mouse_wait_out();
    outb(CMD, CMD_AUX_ENABLE);
    mouse_wait_out();
    outb(CMD, CMD_READ_CFG);
    uint8_t cfg = mouse_read();
    cfg |= CFG_AUX_CLK;
    cfg &= ~CFG_AUX_IRQ;
    mouse_wait_out();
    outb(CMD, CMD_WRITE_CFG);
    mouse_wait_out();
    outb(DATA, cfg);
    mouse_write(0xF6);
    (void)mouse_read();
    mouse_write(0xF4);
    (void)mouse_read();
    mouse_ok = true;
}

void mouse_poll(int *dx, int *dy, int *buttons)
{
    while ((inb(STATUS) & 0x01)) {
        uint8_t b = inb(DATA);
        if (mouse_phase == 0) {
            if ((b & 0x08) == 0) continue;
            mouse_buf[0] = b;
            mouse_phase = 1;
        } else if (mouse_phase == 1) {
            mouse_buf[1] = b;
            mouse_phase = 2;
        } else {
            mouse_buf[2] = b;
            mouse_phase = 0;
            mouse_buttons = (mouse_buf[0] & 0x07);
            mouse_dx += (int)(int8_t)mouse_buf[1];
            mouse_dy -= (int)(int8_t)mouse_buf[2];
        }
    }
    *dx = mouse_dx;
    *dy = mouse_dy;
    *buttons = mouse_buttons;
    mouse_dx = mouse_dy = 0;
}

void mouse_clear(void)
{
    mouse_dx = mouse_dy = mouse_buttons = 0;
    mouse_phase = 0;
    while (inb(STATUS) & 0x01) (void)inb(DATA);
}
