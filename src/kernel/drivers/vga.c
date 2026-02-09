/**
 * VGA text mode driver (80x25, 16 colors)
 * Writes to 0xB8000; each character is 2 bytes (attr << 8 | char).
 */

#include <kernel/vga.h>
#include <kernel/port.h>

static uint16_t *vga_buffer = (uint16_t *)VGA_ADDR;
static size_t vga_row;
static size_t vga_column;
static uint8_t vga_fg;
static uint8_t vga_bg;

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg)
{
    return fg | (bg << 4);
}

static inline uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_clear(void)
{
    vga_row = 0;
    vga_column = 0;
    uint8_t color = vga_entry_color(vga_fg, vga_bg);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buffer[i] = vga_entry(' ', color);
}

void vga_set_color(uint8_t fg, uint8_t bg)
{
    vga_fg = fg;
    vga_bg = bg;
}

static void vga_scroll(void)
{
    for (size_t i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
        vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
    uint8_t color = vga_entry_color(vga_fg, vga_bg);
    for (size_t i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
        vga_buffer[i] = vga_entry(' ', color);
    vga_row = VGA_HEIGHT - 1;
    vga_column = 0;
}

void vga_putchar(char c)
{
    if (c == '\n') {
        vga_column = 0;
        if (++vga_row >= VGA_HEIGHT)
            vga_scroll();
        return;
    }
    if (c == '\r') {
        vga_column = 0;
        return;
    }
    uint8_t color = vga_entry_color(vga_fg, vga_bg);
    size_t idx = vga_row * VGA_WIDTH + vga_column;
    vga_buffer[idx] = vga_entry((unsigned char)c, color);
    if (++vga_column >= VGA_WIDTH) {
        vga_column = 0;
        if (++vga_row >= VGA_HEIGHT)
            vga_scroll();
    }
}

void vga_puts(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}

static void vga_puthex(uint32_t n)
{
    const char hex[] = "0123456789ABCDEF";
    vga_putchar('0');
    vga_putchar('x');
    for (int i = 7; i >= 0; i--)
        vga_putchar(hex[(n >> (i * 4)) & 0xF]);
}

void vga_putdec(uint32_t n)
{
    if (n == 0) {
        vga_putchar('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (n) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (i > 0)
        vga_putchar(buf[--i]);
}
