#ifndef BONFIRE_GUI_DRAW_H
#define BONFIRE_GUI_DRAW_H

#include <kernel/types.h>

#define GUI_FB_W 320
#define GUI_FB_H 200

enum gui_color {
    GUI_C_BLACK,
    GUI_C_BLUE,
    GUI_C_GREEN,
    GUI_C_CYAN,
    GUI_C_RED,
    GUI_C_MAGENTA,
    GUI_C_BROWN,
    GUI_C_LTGRAY,
    GUI_C_DKGRAY,
    GUI_C_BR_BLUE,
    GUI_C_BR_GREEN,
    GUI_C_BR_CYAN,
    GUI_C_BR_RED,
    GUI_C_BR_MAGENTA,
    GUI_C_YELLOW,
    GUI_C_WHITE,
};

void gui_draw_apply_palette(void);
void gui_draw_fill_rect(uint8_t *fb, int x, int y, int w, int h, uint8_t color);
void gui_draw_char(uint8_t *fb, int x, int y, char c, uint8_t fg, uint8_t bg);
void gui_draw_text(uint8_t *fb, int x, int y, const char *s, uint8_t fg, uint8_t bg);
void gui_draw_text_len(uint8_t *fb, int x, int y, const char *s, int len, uint8_t fg, uint8_t bg);
void gui_draw_window(uint8_t *fb, int x, int y, int w, int h, const char *title, bool active);
void gui_draw_button(uint8_t *fb, int x, int y, int w, int h, const char *label, bool pressed, bool highlight);
bool gui_draw_hit(int px, int py, int x, int y, int w, int h);

#endif /* BONFIRE_GUI_DRAW_H */
