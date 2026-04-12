/**
 * IPv4 + ICMP echo reply (for ping).
 */

#include <kernel/types.h>
#include <kernel/net.h>

uint16_t net_checksum16(const void *data, int len);

extern void loopback_xmit(const uint8_t *ip, int len);
void tcp_ipv4_input(const uint8_t *pkt, int len, uint32_t src, uint32_t dst);
static void icmp_input(const uint8_t *ip, int iplen, int ip_hdr_len);

static int icmp_echo_reply_rx;

int net_icmp_reply_count(void)
{
    return icmp_echo_reply_rx;
}

void net_icmp_clear_reply(void)
{
    icmp_echo_reply_rx = 0;
}

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6

#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO       8

static uint32_t rd_ip4(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void wr_ip4(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

void ipv4_input(const uint8_t *pkt, int len)
{
    if (len < 20) return;
    int ihl = (pkt[0] & 0x0F) * 4;
    if (ihl < 20 || len < ihl) return;
    uint8_t proto = pkt[9];
    uint32_t src = rd_ip4(pkt + 12);
    uint32_t dst = rd_ip4(pkt + 16);

    if (proto == IP_PROTO_ICMP)
        icmp_input(pkt, len, ihl);
    else if (proto == IP_PROTO_TCP)
        tcp_ipv4_input(pkt, len, src, dst);
}

static void icmp_input(const uint8_t *ip, int iplen, int ip_hdr_len)
{
    if (iplen < ip_hdr_len + 8) return;
    const uint8_t *icmp = ip + ip_hdr_len;
    if (icmp[0] == ICMP_ECHO_REPLY) {
        icmp_echo_reply_rx++;
        return;
    }
    if (icmp[0] != ICMP_ECHO) return;
    /* Echo request: reply */
    uint8_t out[576];
    int icmp_len = iplen - ip_hdr_len;
    if ((int)sizeof(out) < 20 + icmp_len) return;
    out[0] = 0x45;
    out[1] = 0;
    uint16_t total = (uint16_t)(20 + icmp_len);
    out[2] = (uint8_t)(total >> 8);
    out[3] = (uint8_t)total;
    out[4] = out[5] = 0;
    out[6] = out[7] = 0;
    out[8] = 64;
    out[9] = IP_PROTO_ICMP;
    out[10] = out[11] = 0;
    wr_ip4(out + 12, rd_ip4(ip + 16));
    wr_ip4(out + 16, rd_ip4(ip + 12));
    for (int i = 0; i < icmp_len; i++)
        out[20 + i] = icmp[i];
    out[20 + 0] = ICMP_ECHO_REPLY;
    out[20 + 2] = out[20 + 3] = 0;
    uint16_t csum = net_checksum16(out + 20, icmp_len);
    out[20 + 2] = (uint8_t)(csum >> 8);
    out[20 + 3] = (uint8_t)csum;
    uint16_t hc = net_checksum16(out, 20);
    out[10] = (uint8_t)(hc >> 8);
    out[11] = (uint8_t)hc;
    loopback_xmit(out, 20 + icmp_len);
}

void ipv4_output(uint8_t proto, uint32_t src, uint32_t dst, const uint8_t *payload, int payload_len)
{
    uint8_t pkt[2048];
    int total = 20 + payload_len;
    if (total > (int)sizeof(pkt)) return;
    pkt[0] = 0x45;
    pkt[1] = 0;
    pkt[2] = (uint8_t)(total >> 8);
    pkt[3] = (uint8_t)total;
    pkt[4] = pkt[5] = 0;
    pkt[6] = pkt[7] = 0;
    pkt[8] = 64;
    pkt[9] = proto;
    pkt[10] = pkt[11] = 0;
    wr_ip4(pkt + 12, src);
    wr_ip4(pkt + 16, dst);
    for (int i = 0; i < payload_len; i++)
        pkt[20 + i] = payload[i];
    uint16_t hc = net_checksum16(pkt, 20);
    pkt[10] = (uint8_t)(hc >> 8);
    pkt[11] = (uint8_t)hc;
    loopback_xmit(pkt, total);
}

void icmp_send_echo_request(uint32_t dst)
{
    uint8_t icmp[8];
    icmp[0] = ICMP_ECHO;
    icmp[1] = 0;
    icmp[2] = icmp[3] = 0;
    icmp[4] = 0x12;
    icmp[5] = 0x34;
    icmp[6] = 0;
    icmp[7] = 1;
    uint16_t c = net_checksum16(icmp, 8);
    icmp[2] = (uint8_t)(c >> 8);
    icmp[3] = (uint8_t)c;
    ipv4_output(IP_PROTO_ICMP, dst, dst, icmp, 8);
}
