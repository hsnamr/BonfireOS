/**
 * Minimal TCP: one passive (port 80) and one active (ephemeral) for loopback HTTP.
 */

#include <kernel/types.h>
#include <kernel/net.h>

uint16_t net_checksum16(const void *data, int len);
void ipv4_output(uint8_t proto, uint32_t src, uint32_t dst, const uint8_t *payload, int payload_len);

#define IP_PROTO_TCP 6

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

#define SRV_PORT 80
#define CLI_PORT 49152

#define ST_CLOSED 0
#define ST_LISTEN 1
#define ST_SYN_SENT 2
#define ST_SYN_RCVD 3
#define ST_ESTABLISHED 4

typedef struct {
    int state;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint32_t snd_una, snd_nxt, iss;
    uint32_t rcv_nxt;
    uint8_t rx[4096];
    int rx_len;
} Tcb;

static Tcb srv, cli;

static uint16_t rd16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static uint16_t tcp_cksum(uint32_t saddr, uint32_t daddr, const uint8_t *tcp, int tcp_len)
{
    uint32_t sum = (saddr >> 16) + (saddr & 0xFFFFu);
    sum += (daddr >> 16) + (daddr & 0xFFFFu);
    sum += (uint16_t)IP_PROTO_TCP;
    sum += (uint16_t)tcp_len;
    for (int i = 0; i < tcp_len; i += 2) {
        uint16_t w;
        if (i + 1 < tcp_len)
            w = ((uint16_t)tcp[i] << 8) | tcp[i + 1];
        else
            w = (uint16_t)(tcp[i] << 8);
        sum += w;
    }
    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)~((uint16_t)sum);
}

static void tcp_send_flags(uint32_t src_ip, uint32_t dst_ip, uint16_t sport, uint16_t dport,
                           uint32_t seq, uint32_t ack, uint8_t flags, const uint8_t *data, int dlen)
{
    uint8_t seg[1280];
    int hl = 20;
    int total = hl + dlen;
    wr16(seg + 0, sport);
    wr16(seg + 2, dport);
    seg[4] = (uint8_t)(seq >> 24);
    seg[5] = (uint8_t)(seq >> 16);
    seg[6] = (uint8_t)(seq >> 8);
    seg[7] = (uint8_t)seq;
    seg[8] = (uint8_t)(ack >> 24);
    seg[9] = (uint8_t)(ack >> 16);
    seg[10] = (uint8_t)(ack >> 8);
    seg[11] = (uint8_t)ack;
    seg[12] = 0x50;
    seg[13] = flags;
    wr16(seg + 14, 16384);
    wr16(seg + 16, 0);
    wr16(seg + 18, 0);
    for (int i = 0; i < dlen; i++)
        seg[hl + i] = data[i];
    uint16_t c = tcp_cksum(src_ip, dst_ip, seg, total);
    wr16(seg + 16, c);
    ipv4_output(IP_PROTO_TCP, src_ip, dst_ip, seg, total);
}

static void tcp_send_rst(uint32_t src_ip, uint32_t dst_ip, uint16_t sport, uint16_t dport, uint32_t seq, uint32_t ack)
{
    tcp_send_flags(src_ip, dst_ip, sport, dport, seq, ack, TCP_RST, NULL, 0);
}

void tcp_init(void)
{
    srv.state = ST_LISTEN;
    srv.local_port = SRV_PORT;
    cli.state = ST_CLOSED;
    cli.rx_len = 0;
    srv.rx_len = 0;
}

void tcp_ipv4_input(const uint8_t *pkt, int len, uint32_t src, uint32_t dst)
{
    int ihl = (pkt[0] & 0x0F) * 4;
    if (len < ihl + 20) return;
    const uint8_t *th = pkt + ihl;
    int tlen = len - ihl;
    uint16_t sport = rd16(th + 0);
    uint16_t dport = rd16(th + 2);
    uint32_t seq = ((uint32_t)th[4] << 24) | ((uint32_t)th[5] << 16) | ((uint32_t)th[6] << 8) | th[7];
    uint32_t ack = ((uint32_t)th[8] << 24) | ((uint32_t)th[9] << 16) | ((uint32_t)th[10] << 8) | th[11];
    int off = (th[12] >> 4) * 4;
    if (off < 20 || tlen < off) return;
    uint8_t fl = th[13];
    const uint8_t *payload = th + off;
    int plen = tlen - off;

    /* Server: dst 80 */
    if (dport == SRV_PORT && srv.state == ST_LISTEN && (fl & TCP_SYN) && !(fl & TCP_ACK)) {
        srv.state = ST_SYN_RCVD;
        srv.remote_ip = src;
        srv.remote_port = sport;
        srv.rcv_nxt = seq + 1;
        srv.iss = 50000;
        srv.snd_una = srv.iss;
        srv.snd_nxt = srv.iss + 1;
        tcp_send_flags(dst, src, SRV_PORT, sport, srv.iss, srv.rcv_nxt, TCP_SYN | TCP_ACK, NULL, 0);
        return;
    }
    if (dport == SRV_PORT && srv.state == ST_SYN_RCVD && (fl & TCP_ACK) && !(fl & TCP_SYN)) {
        if (ack == srv.iss + 1) {
            srv.state = ST_ESTABLISHED;
            srv.rcv_nxt = seq + 1;
        }
        return;
    }
    if (dport == SRV_PORT && srv.state == ST_ESTABLISHED && (fl & TCP_PSH)) {
        /* HTTP request */
        srv.rcv_nxt = seq + (uint32_t)plen;
        tcp_send_flags(dst, src, SRV_PORT, sport, srv.snd_nxt, srv.rcv_nxt, TCP_ACK, NULL, 0);
        static const char resp[] =
            "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n"
            "<html><head><title>BonfireOS</title></head><body><h1>Hello from loopback</h1>"
            "<p>Lynx-style host / HTTP test</p></body></html>";
        tcp_send_flags(dst, src, SRV_PORT, sport, srv.snd_nxt, srv.rcv_nxt, TCP_ACK | TCP_PSH,
                       (const uint8_t *)resp, (int)sizeof(resp) - 1);
        srv.snd_nxt += (uint32_t)(sizeof(resp) - 1);
        srv.state = ST_CLOSED;
        return;
    }

    /* Client: dst ephemeral */
    if (dport == CLI_PORT && cli.state == ST_SYN_SENT && (fl & TCP_SYN) && (fl & TCP_ACK)) {
        cli.rcv_nxt = seq + 1;
        cli.snd_nxt = ack;
        tcp_send_flags(dst, src, CLI_PORT, sport, cli.snd_nxt, cli.rcv_nxt, TCP_ACK, NULL, 0);
        cli.state = ST_ESTABLISHED;
        return;
    }
    if (dport == CLI_PORT && cli.state == ST_ESTABLISHED && (fl & TCP_PSH)) {
        /* Response data */
        if (plen + cli.rx_len < (int)sizeof(cli.rx)) {
            for (int i = 0; i < plen; i++)
                cli.rx[cli.rx_len++] = payload[i];
        }
        cli.rcv_nxt = seq + (uint32_t)plen;
        tcp_send_flags(dst, src, CLI_PORT, sport, cli.snd_nxt, cli.rcv_nxt, TCP_ACK, NULL, 0);
        return;
    }
    if (dport == CLI_PORT && (fl & TCP_FIN) && cli.state == ST_ESTABLISHED) {
        cli.state = ST_CLOSED;
        return;
    }

}

void tcp_reset(void)
{
    tcp_init();
}

int tcp_connect_send_get(uint32_t ip)
{
    uint32_t loop = NET_IPV4_LOOPBACK;
    cli.state = ST_SYN_SENT;
    cli.local_port = CLI_PORT;
    cli.remote_ip = ip;
    cli.remote_port = SRV_PORT;
    cli.iss = 10000;
    cli.snd_una = cli.iss;
    cli.snd_nxt = cli.iss + 1;
    cli.rx_len = 0;
    srv.state = ST_LISTEN;
    srv.rx_len = 0;
    tcp_send_flags(loop, loop, CLI_PORT, SRV_PORT, cli.iss, 0, TCP_SYN, NULL, 0);
    return 0;
}

int tcp_send_data(const uint8_t *data, int len)
{
    uint32_t loop = NET_IPV4_LOOPBACK;
    if (cli.state != ST_ESTABLISHED) return -1;
    tcp_send_flags(loop, loop, CLI_PORT, SRV_PORT, cli.snd_nxt, cli.rcv_nxt, TCP_ACK | TCP_PSH, data, len);
    cli.snd_nxt += (uint32_t)len;
    return 0;
}

int tcp_client_rx_len(void)
{
    return cli.rx_len;
}

const uint8_t *tcp_client_rx_buf(void)
{
    return cli.rx;
}
