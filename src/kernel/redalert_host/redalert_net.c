/**
 * Red Alert network host (GCL / IPX replacement).
 * Stub implementation: no NIC or IPX stack in BonfireOS yet.
 * Port can call these; they no-op so linking succeeds.
 * Replace with real driver when networking is added.
 */

#include <kernel/redalert_host.h>
#include <kernel/types.h>

static bool net_inited;

int redalert_net_init_impl(void)
{
    net_inited = true;
    return 0; /* success */
}

void redalert_net_shutdown_impl(void)
{
    net_inited = false;
}

int redalert_net_broadcast_impl(const void *data, size_t len)
{
    (void)data;
    (void)len;
    if (len > REDALERT_NET_PACKET_MAX) return -1;
    return 0; /* no peers */
}

int redalert_net_send_impl(uint8_t peer_id, const void *data, size_t len)
{
    (void)peer_id;
    (void)data;
    (void)len;
    if (len > REDALERT_NET_PACKET_MAX) return -1;
    return 0;
}

int redalert_net_receive_impl(void *buf, size_t max_len, uint8_t *from_peer_id)
{
    (void)buf;
    (void)max_len;
    (void)from_peer_id;
    return 0; /* no data */
}

int redalert_net_peer_count_impl(void)
{
    return 0;
}

void redalert_net_get_peer_address_impl(uint8_t peer_id, char *out, size_t out_size)
{
    (void)peer_id;
    if (out && out_size > 0) out[0] = '\0';
    (void)out_size;
}
