/**
 * PIT (8253/8254) timer - channel 0, periodic interrupt.
 * Frequency = 1193182 / divisor; e.g. 11932 -> ~100 Hz.
 */

#include <kernel/timer.h>
#include <kernel/port.h>

#define PIT_CH0    0x40
#define PIT_CMD    0x43
#define PIT_SQUARE 0x36
#define PIT_HZ     1193182

static volatile uint32_t timer_ms;

void timer_init(unsigned hz)
{
    timer_ms = 0;
    uint32_t divisor = PIT_HZ / hz;
    if (divisor > 65535) divisor = 65535;
    outb(PIT_CMD, PIT_SQUARE);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_tick(void)
{
    timer_ms += 10; /* 100 Hz -> 10 ms per tick */
}

uint32_t timer_get_ms(void)
{
    return timer_ms;
}
