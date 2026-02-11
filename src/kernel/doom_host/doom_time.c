#include <kernel/timer.h>
#include <kernel/types.h>

uint32_t doom_time_ms_impl(void)
{
    return timer_get_ms();
}

void doom_time_delay_ms_impl(uint32_t ms)
{
    uint32_t end = timer_get_ms() + ms;
    while (timer_get_ms() < end)
        __asm__ volatile ("hlt");
}
