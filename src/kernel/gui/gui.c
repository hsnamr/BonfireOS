/**
 * Optional 1980s-style GUI for BonfireOS.
 * Uses VGA Mode 13h (320x200, 256 colors) with a CGA/EGA-inspired palette.
 * Chunky windows, thick borders, title bar — launch from CLI with "gui".
 */

#include <kernel/gui.h>
#include <kernel/video_mode13.h>
#include <kernel/keyboard.h>
#include <kernel/types.h>

#define W   320
#define H   200

/* CGA/EGA-style 16-color palette (RGB 0–63 per channel) */
static const uint8_t gui_palette[256 * 3] = {
    /* 0–15: classic CGA */
    0,   0,   0,   /*  0 black */
    0,   0,  42,   /*  1 blue */
    0,  42,   0,   /*  2 green */
    0,  42,  42,   /*  3 cyan */
    42,  0,   0,   /*  4 red */
    42,  0,  42,   /*  5 magenta */
    42, 21,   0,   /*  6 brown */
    42, 42,  42,   /*  7 light gray */
    21, 21,  21,   /*  8 dark gray */
    21, 21,  63,   /*  9 bright blue */
    21, 63,  21,   /* 10 bright green */
    21, 63,  63,   /* 11 bright cyan */
    63, 21,  21,   /* 12 bright red */
    63, 21,  63,   /* 13 bright magenta */
    63, 63,  21,   /* 14 yellow */
    63, 63,  63,   /* 15 white */
};

enum {
    C_BLACK,
    C_BLUE,
    C_GREEN,
    C_CYAN,
    C_RED,
    C_MAGENTA,
    C_BROWN,
    C_LTGRAY,
    C_DKGRAY,
    C_BR_BLUE,
    C_BR_GREEN,
    C_BR_CYAN,
    C_BR_RED,
    C_BR_MAGENTA,
    C_YELLOW,
    C_WHITE,
};

static void fill_rect(uint8_t *fb, int x0, int y0, int w, int h, uint8_t color)
{
    if (x0 < 0) { w += x0; x0 = 0; }
    if (y0 < 0) { h += y0; y0 = 0; }
    if (x0 + w > W) w = W - x0;
    if (y0 + h > H) h = H - y0;
    if (w <= 0 || h <= 0) return;
    for (int y = 0; y < h; y++) {
        uint8_t *row = fb + (y0 + y) * W + x0;
        for (int x = 0; x < w; x++)
            row[x] = color;
    }
}

/* 8x8 chunky font: each byte is a row, MSB = left pixel (1 = foreground) */
static const uint8_t font_8x8[256][8] = {
    [0] = { 0, 0, 0, 0, 0, 0, 0, 0 },
    [' '] = { 0, 0, 0, 0, 0, 0, 0, 0 },
    ['B'] = { 0x7E, 0x63, 0x63, 0x7F, 0x63, 0x63, 0x7E, 0 },
    ['o'] = { 0, 0, 0x3E, 0x63, 0x63, 0x63, 0x3E, 0 },
    ['n'] = { 0, 0, 0x6E, 0x73, 0x63, 0x63, 0x63, 0 },
    ['f'] = { 0x1C, 0x36, 0x06, 0x1F, 0x06, 0x06, 0x06, 0 },
    ['i'] = { 0, 0x0C, 0, 0x0C, 0x0C, 0x0C, 0x0C, 0 },
    ['r'] = { 0, 0, 0x6E, 0x33, 0x03, 0x03, 0x03, 0 },
    ['e'] = { 0, 0, 0x3E, 0x63, 0x7F, 0x03, 0x3E, 0 },
    ['O'] = { 0x3C, 0x66, 0x63, 0x63, 0x63, 0x66, 0x3C, 0 },
    ['S'] = { 0x3E, 0x63, 0x03, 0x3E, 0x60, 0x63, 0x3E, 0 },
    ['G'] = { 0x3C, 0x66, 0x03, 0x73, 0x63, 0x66, 0x7C, 0 },
    ['U'] = { 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0 },
    ['I'] = { 0x3F, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0 },
    ['P'] = { 0x7F, 0x63, 0x63, 0x7F, 0x03, 0x03, 0x03, 0 },
    ['a'] = { 0, 0, 0x3E, 0x60, 0x7E, 0x63, 0x7E, 0 },
    ['k'] = { 0, 0x63, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0 },
    ['y'] = { 0, 0, 0x63, 0x63, 0x63, 0x62, 0x3C, 0 },
    ['t'] = { 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x0C, 0x38, 0 },
    ['u'] = { 0, 0, 0x63, 0x63, 0x63, 0x67, 0x3B, 0 },
    ['s'] = { 0, 0, 0x3E, 0x03, 0x3E, 0x60, 0x3E, 0 },
    ['h'] = { 0, 0x63, 0x63, 0x73, 0x6B, 0x67, 0x63, 0 },
    ['l'] = { 0, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0 },
    ['c'] = { 0, 0, 0x3E, 0x63, 0x03, 0x63, 0x3E, 0 },
    ['C'] = { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0 },
    ['L'] = { 0x03, 0x03, 0x03, 0x03, 0x03, 0x63, 0x7F, 0 },
    ['R'] = { 0x6E, 0x33, 0x33, 0x3E, 0x36, 0x33, 0x73, 0 },
    ['('] = { 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0 },
    [')'] = { 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0 },
    ['['] = { 0x3F, 0x03, 0x03, 0x03, 0x03, 0x03, 0x3F, 0 },
    [']'] = { 0x3F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3F, 0 },
    ['1'] = { 0x0C, 0x1C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0 },
    ['9'] = { 0x3E, 0x63, 0x63, 0x3F, 0x60, 0x63, 0x3E, 0 },
    ['8'] = { 0x3E, 0x63, 0x63, 0x3E, 0x63, 0x63, 0x3E, 0 },
    ['5'] = { 0x7F, 0x03, 0x03, 0x3E, 0x60, 0x63, 0x3E, 0 },
    ['.'] = { 0, 0, 0, 0, 0, 0x0C, 0x0C, 0 },
};

static void draw_char_8x8(uint8_t *fb, int x, int y, char c, uint8_t fg, uint8_t bg)
{
    if (c < 0 || c >= 128) c = ' ';
    const uint8_t *glyph = font_8x8[(unsigned char)c];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint8_t color = (bits & 0x80) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < W && py >= 0 && py < H)
                fb[py * W + px] = color;
            bits <<= 1;
        }
    }
}

static void draw_string(uint8_t *fb, int x, int y, const char *s, uint8_t fg, uint8_t bg)
{
    while (*s) {
        draw_char_8x8(fb, x, y, *s++, fg, bg);
        x += 8;
    }
}

void gui_run(void)
{
    video_mode13_enter();
    uint8_t *fb = video_mode13_framebuffer();
    video_mode13_set_palette(gui_palette);

    /* Desktop: blue background */
    fill_rect(fb, 0, 0, W, H, C_BLUE);

    /* Main window: chunky 1980s style (thick 3D border) */
    int win_x = 24;
    int win_y = 20;
    int win_w = 272;
    int win_h = 120;
    int border = 4;

    /* Outer shadow (dark) */
    fill_rect(fb, win_x + border, win_y + border, win_w, win_h, C_DKGRAY);
    /* Inner window fill */
    fill_rect(fb, win_x, win_y, win_w, win_h, C_LTGRAY);
    /* Top/left highlight (white) */
    fill_rect(fb, win_x, win_y, win_w, 2, C_WHITE);
    fill_rect(fb, win_x, win_y, 2, win_h, C_WHITE);
    /* Bottom/right shadow (dark gray) */
    fill_rect(fb, win_x, win_y + win_h - 2, win_w, 2, C_DKGRAY);
    fill_rect(fb, win_x + win_w - 2, win_y, 2, win_h, C_DKGRAY);
    /* Title bar: inverse (white on blue) */
    fill_rect(fb, win_x + 2, win_y + 2, win_w - 4, 14, C_BR_BLUE);
    draw_string(fb, win_x + 8, win_y + 5, " BonfireOS GUI ", C_WHITE, C_BR_BLUE);
    /* Inner content area */
    fill_rect(fb, win_x + 4, win_y + 20, win_w - 8, win_h - 24, C_LTGRAY);
    draw_string(fb, win_x + 12, win_y + 36, "Press any key to return to CLI", C_BLACK, C_LTGRAY);
    draw_string(fb, win_x + 12, win_y + 52, "Welcome to the 1980s.", C_BLUE, C_LTGRAY);

    /* Small decorative stripe at bottom (CGA style) */
    fill_rect(fb, 0, H - 12, W, 12, C_BR_BLUE);
    draw_string(fb, 8, H - 10, "BonfireOS (c) 1985  [gui]", C_WHITE, C_BR_BLUE);

    /* Event loop: exit on any key */
    for (;;) {
        char c = keyboard_getchar();
        if (c)
            break;
        __asm__ volatile ("hlt");
    }

    video_mode13_leave();
}
