/**
 * Network stack glue: loopback, poll, ping, HTTP over loopback TCP.
 */

#include <kernel/types.h>
#include <kernel/net.h>

extern void ipv4_input(const uint8_t *pkt, int len);
extern int loopback_fetch(uint8_t *out, int max);
extern void loopback_clear(void);
extern void icmp_send_echo_request(uint32_t dst);
extern void net_icmp_clear_reply(void);
extern int net_icmp_reply_count(void);
extern void tcp_init(void);
extern void tcp_reset(void);
extern int tcp_connect_send_get(uint32_t ip);
extern int tcp_send_data(const uint8_t *data, int len);
extern int tcp_client_rx_len(void);
extern const uint8_t *tcp_client_rx_buf(void);

void net_poll(void)
{
    uint8_t buf[2048];
    int n;
    while ((n = loopback_fetch(buf, (int)sizeof(buf))) > 0)
        ipv4_input(buf, n);
}

void net_init(void)
{
    loopback_clear();
    net_icmp_clear_reply();
    tcp_init();
}

int net_ping(uint32_t dst)
{
    net_icmp_clear_reply();
    icmp_send_echo_request(dst);
    for (int i = 0; i < 8000; i++)
        net_poll();
    return net_icmp_reply_count() > 0 ? 0 : -1;
}

int net_http_get_loopback(char *out, size_t max_out)
{
    if (!out || max_out < 2)
        return -1;
    tcp_reset();
    tcp_connect_send_get(NET_IPV4_LOOPBACK);
    for (int i = 0; i < 8000; i++)
        net_poll();
    static const char req[] = "GET / HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
    if (tcp_send_data((const uint8_t *)req, (int)sizeof(req) - 1) != 0) {
        out[0] = '\0';
        return -1;
    }
    for (int i = 0; i < 8000; i++)
        net_poll();
    int n = tcp_client_rx_len();
    if (n <= 0) {
        out[0] = '\0';
        return -1;
    }
    const uint8_t *rx = tcp_client_rx_buf();
    int body = 0;
    for (int i = 0; i + 3 < n; i++) {
        if (rx[i] == '\r' && rx[i + 1] == '\n' && rx[i + 2] == '\r' && rx[i + 3] == '\n') {
            body = i + 4;
            break;
        }
    }
    int j = 0;
    for (int i = body; i < n && j < (int)max_out - 1; i++)
        out[j++] = (char)rx[i];
    out[j] = '\0';
    return j;
}
