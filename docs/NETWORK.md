# Networking in BonfireOS

## What is included

The kernel includes a **minimal IPv4 stack** with:

- **Loopback** interface (queues IPv4 datagrams in memory).
- **ICMP** echo request/reply (`ping` to 127.0.0.1).
- **TCP** (single client + single passive server on port 80) for **HTTP/1.0** over loopback.
- No physical NIC driver yet: there is no PCI/MMIO mapping for hardware in the default boot path, so **WAN traffic is not available** without adding a driver (e.g. virtio-net or e1000) and extending identity paging for device BARs.

## Build

Networking is enabled by default (`ENABLE_NET=1` in the Makefile). To build without it:

```bash
make ENABLE_NET=0
```

## Shell commands

- **`ping`** — Sends an ICMP echo to `127.0.0.1` via loopback. Prints `ok` or `no reply`.
- **`httpget`** — Performs `GET /` over TCP to the built-in HTTP server on loopback and prints the response body (HTML).
- **`LYNX`** — Lynx-style host: fetches `http://127.0.0.1/` and prints the body (see [LYNX_PORT.md](LYNX_PORT.md)).

## Architecture

Sources live under `src/kernel/net/`:

| File            | Role                                      |
|----------------|-------------------------------------------|
| `net_checksum.c` | Internet checksum (RFC 1071)          |
| `net_loopback.c` | RX queue for loopback                  |
| `net_ipv4.c`     | IPv4 RX/TX, ICMP echo reply            |
| `net_tcp.c`      | Minimal TCP + HTTP-sized response      |
| `net.c`          | `net_init`, `net_poll`, `ping`, HTTP   |

`net_poll()` should be called to drain the loopback queue; the shell commands call it internally in a loop.

## Future work

- PCI configuration space access and a **virtio-net** or **e1000** driver for QEMU.
- Extend page tables so MMIO regions for NIC BARs are mapped.
- ARP, routing table, DNS resolver, full TCP (multiple sockets, retransmits).
- TLS (would require a crypto library and significant memory).
