#ifndef BONFIRE_LYNX_HOST_H
#define BONFIRE_LYNX_HOST_H

/**
 * Lynx-style text-mode browser host for BonfireOS.
 * This is not the full upstream Lynx tree; it uses the in-kernel HTTP client
 * when a URL is supported (see docs/LYNX_PORT.md).
 */
int lynx_main(int argc, char **argv);

#endif /* BONFIRE_LYNX_HOST_H */
