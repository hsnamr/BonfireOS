#ifndef BONFIRE_NET_STACK_H
#define BONFIRE_NET_STACK_H

#include <kernel/types.h>

#define NET_IPV4_LOOPBACK 0x7F000001u

void net_init(void);
void net_poll(void);

/* ICMP echo to IPv4 address (loopback only unless a NIC driver is present). */
int net_ping(uint32_t dst_ip);

/* HTTP/1.0 GET http://127.0.0.1/ via loopback TCP + built-in tiny server. */
int net_http_get_loopback(char *out, size_t max_out);

#endif /* BONFIRE_NET_STACK_H */
