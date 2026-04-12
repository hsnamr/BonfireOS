# Lynx and BonfireOS

## What “Lynx” means here

**Lynx** is a classic text-mode web browser (https://lynx.invisible-island.net/). The full program depends on:

- A POSIX-like OS (files, processes, environment).
- **curses** / terminal capability libraries.
- **OpenSSL** or similar for HTTPS.
- The **libwww** stack and many configure-time options.

BonfireOS does not provide a full Unix userspace, so **the upstream Lynx source tree is not linked into the kernel**.

Instead, this repository provides:

1. **`lynx_main()`** in `src/kernel/lynx_host/lynx_host.c` — a small **Lynx-style host** that uses the in-kernel HTTP client (`net_http_get_loopback`) to display a page on the VGA text console.
2. Shell command **`LYNX`** — runs that host (same idea as `DOOM` / `REDALERT` integration).

That gives a **text-mode browsing experience** aligned with Lynx’s role, without porting tens of thousands of lines of dependencies.

## Porting real Lynx (roadmap)

To run actual Lynx on BonfireOS you would typically:

1. **Userspace** — A libc (newlib + syscalls), a terminal layer, and a socket API wired to the kernel TCP/IP stack.
2. **Networking** — Physical NIC driver + ARP/DNS + robust TCP.
3. **Build** — Cross-compile Lynx with `--disable-nls`, stub or port curses, and disable HTTPS initially or add mbedTLS/OpenSSL.
4. **Memory** — Lynx expects a multi-megabyte heap; the kernel heap would need to grow or Lynx would run as a user process with its own allocator.

Until then, use **`LYNX`** or **`httpget`** for loopback HTTP, and see [NETWORK.md](NETWORK.md) for the stack’s limits.
