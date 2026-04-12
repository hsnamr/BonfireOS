/**
 * Lynx-style text browser host: uses the in-kernel HTTP client on loopback.
 * Full upstream Lynx is not linked here; see docs/LYNX_PORT.md.
 */

#include <kernel/lynx_host.h>
#include <kernel/net.h>
#include <kernel/vga.h>
#include <kernel/types.h>

static int str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

int lynx_main(int argc, char **argv)
{
    const char *url = "http://127.0.0.1/";
    if (argc > 1 && argv[1] && argv[1][0])
        url = argv[1];

    if (!str_eq(url, "http://127.0.0.1/") && !str_eq(url, "http://127.0.0.1")) {
        vga_puts("Lynx host: only http://127.0.0.1/ is supported (loopback).\n");
        vga_puts("A real NIC driver and DNS are not in this build.\n");
        return -1;
    }

    static char buf[4096];
    int n = net_http_get_loopback(buf, sizeof(buf));
    if (n < 0) {
        vga_puts("Lynx: HTTP fetch failed (is the network stack enabled?)\n");
        return -1;
    }
    vga_puts(buf);
    vga_putchar('\n');
    return 0;
}
