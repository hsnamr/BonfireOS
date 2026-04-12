/**
 * Internet checksum (RFC 1071).
 */

#include <kernel/types.h>

uint16_t net_checksum16(const void *data, int len)
{
    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t *)data;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len) {
        sum += (uint16_t)(*(const uint8_t *)p) << 8;
    }
    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFFu);
}
