/**
 * Loopback NIC: queues IPv4 datagrams for net_poll().
 */

#include <kernel/types.h>

#define LB_Q 16
#define LB_MTU 2048

static uint8_t lb_buf[LB_Q][LB_MTU];
static int lb_len[LB_Q];
static int lb_head, lb_tail;

void loopback_xmit(const uint8_t *ip, int len)
{
    if (len <= 0 || len > LB_MTU) return;
    int next = (lb_tail + 1) % LB_Q;
    if (next == lb_head)
        return;
    for (int i = 0; i < len; i++)
        lb_buf[lb_tail][i] = ip[i];
    lb_len[lb_tail] = len;
    lb_tail = next;
}

int loopback_fetch(uint8_t *out, int max)
{
    if (lb_head == lb_tail) return 0;
    int n = lb_len[lb_head];
    if (n > max) n = max;
    for (int i = 0; i < n; i++)
        out[i] = lb_buf[lb_head][i];
    lb_head = (lb_head + 1) % LB_Q;
    return n;
}

void loopback_clear(void)
{
    lb_head = lb_tail = 0;
}
