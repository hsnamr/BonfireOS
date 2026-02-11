#ifndef BONFIRE_TIMER_H
#define BONFIRE_TIMER_H

#include <kernel/types.h>

void timer_init(unsigned hz);
void timer_tick(void);       /* call from IRQ0 handler */
uint32_t timer_get_ms(void); /* milliseconds since boot */

#endif /* BONFIRE_TIMER_H */
