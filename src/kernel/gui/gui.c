/**
 * GEM / Windows 3 style desktop: Program Manager, File Manager, utilities.
 */

#include <kernel/gui.h>
#include <kernel/gui_draw.h>
#include <kernel/video_mode13.h>
#include <kernel/keyboard.h>
#include <kernel/mouse.h>
#include <kernel/timer.h>
#include <kernel/fs.h>
#include <kernel/types.h>

typedef enum {
    APP_NONE = 0,
    APP_PROGRAM_MANAGER,
    APP_FILE_MANAGER,
    APP_CALCULATOR,
    APP_NOTEPAD,
    APP_CLOCK,
    APP_SNAKE,
} AppId;

#define MAX_STACK 8
#define FM_MAX_NAMES 32
#define NOTE_BUF 4096
#define CALC_MAX 24
#define SNAKE_MAX 64
#define GRID_W 20
#define GRID_H 12
#define CELL 6

static AppId stack[MAX_STACK];
static int stack_n;

static int mx, my;
static int prev_btn;

static size_t str_len(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void str_cpy(char *d, const char *s, size_t max)
{
    size_t i = 0;
    while (s[i] && i + 1 < max) { d[i] = s[i]; i++; }
    d[i] = '\0';
}

static void path_join(char *out, const char *cwd, const char *name, int strip_slash)
{
    size_t i = 0;
    while (cwd[i] && i < FS_PATH_MAX - 1) { out[i] = cwd[i]; i++; }
    if (i == 0 || out[i - 1] != '/') out[i++] = '/';
    size_t j = 0;
    while (name[j] && i < FS_PATH_MAX - 1) {
        if (strip_slash && name[j] == '/') break;
        out[i++] = name[j++];
    }
    if (strip_slash && i > 0 && out[i - 1] == '/') i--;
    out[i] = '\0';
}

static struct {
    int pm_sel;
    char fm_cwd[FS_PATH_MAX];
    int fm_count;
    char fm_names[FM_MAX_NAMES][FS_NAME_MAX + 4];
    int fm_sel;
    char calc_disp[CALC_MAX];
    char calc_entry[CALC_MAX];
    int calc_op;
    long long calc_acc;
    int calc_have_acc;
    char note_buf[NOTE_BUF];
    int note_len;
    int note_cursor;
    char note_path[FS_PATH_MAX];
    int clk_mode;
    uint32_t clk_sw_start;
    uint32_t clk_sw_acc;
    int clk_sw_running;
    uint32_t clk_timer_deadline;
    int clk_timer_active;
    char clk_digits[8];
    int clk_digit_len;
    uint32_t clk_alarm_at;
    int clk_alarm_armed;
    int snake_x[SNAKE_MAX], snake_y[SNAKE_MAX];
    int snake_len;
    int snake_dir;
    int snake_fx, snake_fy;
    uint32_t snake_next_tick;
    int snake_alive;
} G;

static void stack_push(AppId a)
{
    if (stack_n < MAX_STACK)
        stack[stack_n++] = a;
}

static void stack_pop(void)
{
    if (stack_n > 0)
        stack_n--;
}

static AppId stack_top(void)
{
    return stack_n ? stack[stack_n - 1] : APP_NONE;
}

static void fm_refresh(void)
{
    char listbuf[512];
    fs_get_cwd(G.fm_cwd, sizeof(G.fm_cwd));
    if (fs_list(NULL, listbuf, sizeof(listbuf)) != 0) {
        G.fm_count = 0;
        return;
    }
    G.fm_count = 0;
    const char *p = listbuf;
    while (*p && G.fm_count < FM_MAX_NAMES) {
        while (*p == ' ') p++;
        if (!*p) break;
        size_t i = 0;
        while (*p && *p != ' ' && i < sizeof(G.fm_names[0]) - 1)
            G.fm_names[G.fm_count][i++] = *p++;
        G.fm_names[G.fm_count][i] = '\0';
        G.fm_count++;
    }
}

static void fm_parent(void)
{
    size_t n = str_len(G.fm_cwd);
    if (n <= 1) return;
    while (n > 1 && G.fm_cwd[n - 1] != '/') n--;
    if (n == 0) {
        G.fm_cwd[0] = '/';
        G.fm_cwd[1] = '\0';
    } else {
        G.fm_cwd[n] = '\0';
    }
    fs_chdir(G.fm_cwd);
    fm_refresh();
    G.fm_sel = 0;
}

static void fm_open_sel(void)
{
    if (G.fm_sel < 0 || G.fm_sel >= G.fm_count) return;
    char *name = G.fm_names[G.fm_sel];
    size_t nl = str_len(name);
    if (nl > 0 && name[nl - 1] == '/') {
        char path[FS_PATH_MAX];
        path_join(path, G.fm_cwd, name, 1);
        if (fs_chdir(path) == 0) {
            fm_refresh();
            G.fm_sel = 0;
        }
    } else {
        char path[FS_PATH_MAX];
        path_join(path, G.fm_cwd, name, 0);
        int n = fs_read(path, G.note_buf, NOTE_BUF - 1);
        if (n < 0) n = 0;
        G.note_buf[n] = '\0';
        G.note_len = n;
        G.note_cursor = 0;
        str_cpy(G.note_path, path, sizeof(G.note_path));
        stack_push(APP_NOTEPAD);
    }
}

static void calc_reset(void)
{
    G.calc_disp[0] = '0';
    G.calc_disp[1] = '\0';
    G.calc_entry[0] = '\0';
    G.calc_op = 0;
    G.calc_acc = 0;
    G.calc_have_acc = 0;
}

static long long calc_parse_int(void)
{
    long long v = 0;
    int neg = 0;
    const char *p = G.calc_disp;
    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    return neg ? -v : v;
}

static void calc_fmt_ll(long long v)
{
    int n = 0;
    char tmp[24];
    int i = 0;
    if (v == 0) { G.calc_disp[n++] = '0'; G.calc_disp[n] = '\0'; return; }
    int neg = v < 0;
    unsigned long long u = (unsigned long long)(neg ? -v : v);
    while (u) { tmp[i++] = (char)('0' + (u % 10)); u /= 10; }
    if (neg) G.calc_disp[n++] = '-';
    while (i > 0) G.calc_disp[n++] = tmp[--i];
    G.calc_disp[n] = '\0';
}

static void calc_apply(void)
{
    long long v = calc_parse_int();
    if (!G.calc_have_acc) {
        G.calc_acc = v;
        G.calc_have_acc = 1;
        return;
    }
    if (G.calc_op == 0) {
        G.calc_acc = v;
        calc_fmt_ll(G.calc_acc);
        return;
    }
    if (G.calc_op == '+') G.calc_acc += v;
    else if (G.calc_op == '-') G.calc_acc -= v;
    else if (G.calc_op == '*') G.calc_acc *= v;
    else if (G.calc_op == '/') {
        if (v != 0) G.calc_acc /= v;
    }
    calc_fmt_ll(G.calc_acc);
}

static void snake_reset(void)
{
    G.snake_len = 3;
    G.snake_x[0] = 8; G.snake_y[0] = 6;
    G.snake_x[1] = 7; G.snake_y[1] = 6;
    G.snake_x[2] = 6; G.snake_y[2] = 6;
    G.snake_dir = 1;
    G.snake_fx = 12;
    G.snake_fy = 6;
    G.snake_next_tick = timer_get_ms() + 220;
    G.snake_alive = 1;
}

static void snake_tick(uint8_t *fb, int gx, int gy)
{
    if (!G.snake_alive) return;
    uint32_t now = timer_get_ms();
    if ((int32_t)(now - G.snake_next_tick) < 0) return;
    G.snake_next_tick = now + 220;
    int dx = 0, dy = 0;
    if (G.snake_dir == 0) { dy = -1; }
    if (G.snake_dir == 1) { dx = 1; }
    if (G.snake_dir == 2) { dy = 1; }
    if (G.snake_dir == 3) { dx = -1; }
    int nx = G.snake_x[0] + dx;
    int ny = G.snake_y[0] + dy;
    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) { G.snake_alive = 0; return; }
    for (int i = 0; i < G.snake_len; i++)
        if (G.snake_x[i] == nx && G.snake_y[i] == ny) { G.snake_alive = 0; return; }
    for (int i = G.snake_len; i > 0; i--) {
        G.snake_x[i] = G.snake_x[i - 1];
        G.snake_y[i] = G.snake_y[i - 1];
    }
    G.snake_x[0] = nx;
    G.snake_y[0] = ny;
    if (nx == G.snake_fx && ny == G.snake_fy) {
        if (G.snake_len < SNAKE_MAX - 1) G.snake_len++;
        G.snake_fx = (G.snake_fx + 7) % GRID_W;
        G.snake_fy = (G.snake_fy + 3) % GRID_H;
    }
    (void)fb;
    (void)gx;
    (void)gy;
}

static void snake_draw(uint8_t *fb, int gx, int gy)
{
    gui_draw_fill_rect(fb, gx, gy, GRID_W * CELL, GRID_H * CELL, GUI_C_BLACK);
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            gui_draw_fill_rect(fb, gx + x * CELL, gy + y * CELL, CELL - 1, CELL - 1, GUI_C_DKGRAY);
    if (G.snake_alive) {
        gui_draw_fill_rect(fb, gx + G.snake_fx * CELL, gy + G.snake_fy * CELL, CELL - 1, CELL - 1, GUI_C_BR_RED);
        for (int i = 0; i < G.snake_len; i++)
            gui_draw_fill_rect(fb, gx + G.snake_x[i] * CELL, gy + G.snake_y[i] * CELL, CELL - 1, CELL - 1,
                               i == 0 ? GUI_C_BR_GREEN : GUI_C_GREEN);
    } else
        gui_draw_text(fb, gx + 20, gy + GRID_H * CELL / 2 - 4, "GAME OVER", GUI_C_BR_RED, GUI_C_BLACK);
    gui_draw_text(fb, gx, gy + GRID_H * CELL + 2, "WASD move  R restart", GUI_C_BLACK, GUI_C_LTGRAY);
}

static void fmt_uint(char *buf, uint32_t v)
{
    int i = 0;
    char t[12];
    int n = 0;
    if (v == 0) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n > 0) buf[i++] = t[--n];
    buf[i] = '\0';
}

#define NOTE_COLS 32
#define NOTE_ROWS 10

static void note_index_to_rc(int index, int *pr, int *pc)
{
    int r = 0, c = 0;
    for (int i = 0; i < index && i < G.note_len; i++) {
        char ch = G.note_buf[i];
        if (ch == '\n') {
            r++;
            c = 0;
        } else {
            c++;
            if (c >= NOTE_COLS) {
                r++;
                c = 0;
            }
        }
    }
    *pr = r;
    *pc = c;
}

static void win_offset(int z, int *wx, int *wy)
{
    *wx = 12 + z * 10;
    *wy = 16 + z * 8;
}

static void draw_desktop(uint8_t *fb)
{
    gui_draw_fill_rect(fb, 0, 0, GUI_FB_W, GUI_FB_H, GUI_C_BR_BLUE);
    gui_draw_fill_rect(fb, 0, GUI_FB_H - 14, GUI_FB_W, 14, GUI_C_DKGRAY);
    gui_draw_text(fb, 4, GUI_FB_H - 12, "ESC close/quit  ` Program Mgr", GUI_C_WHITE, GUI_C_DKGRAY);

    for (int z = 0; z < stack_n; z++) {
        AppId app = stack[z];
        int depth = z;
        int wx = 12 + depth * 10;
        int wy = 16 + depth * 8;
        bool top = (z == stack_n - 1);

        if (app == APP_PROGRAM_MANAGER) {
            int ww = 220, wh = 130;
            gui_draw_window(fb, wx, wy, ww, wh, "Program Manager", top);
            const char *items[] = { "File Manager", "Calculator", "Notepad", "Clock", "Snake" };
            for (int i = 0; i < 5; i++) {
                int bx = wx + 8, by = wy + 18 + i * 20, bw = ww - 16, bh = 16;
                gui_draw_button(fb, bx, by, bw, bh, items[i], false, G.pm_sel == i && top);
            }
        } else if (app == APP_FILE_MANAGER) {
            int ww = 260, wh = 140;
            gui_draw_window(fb, wx, wy, ww, wh, "File Manager", top);
            gui_draw_text(fb, wx + 6, wy + 18, "PATH:", GUI_C_BLACK, GUI_C_LTGRAY);
            gui_draw_text_len(fb, wx + 50, wy + 18, G.fm_cwd, (int)str_len(G.fm_cwd), GUI_C_BLACK, GUI_C_LTGRAY);
            gui_draw_text(fb, wx + 6, wy + 30, "U=up ENT=open TAB=sel", GUI_C_DKGRAY, GUI_C_LTGRAY);
            int row = 0;
            for (int i = 0; i < G.fm_count && row < 8; i++) {
                uint8_t bg = (i == G.fm_sel && top) ? GUI_C_BR_CYAN : GUI_C_LTGRAY;
                gui_draw_text_len(fb, wx + 8, wy + 44 + row * 10, G.fm_names[i], (int)str_len(G.fm_names[i]), GUI_C_BLACK, bg);
                row++;
            }
        } else if (app == APP_CALCULATOR) {
            int ww = 200, wh = 118;
            gui_draw_window(fb, wx, wy, ww, wh, "Calculator", top);
            gui_draw_fill_rect(fb, wx + 6, wy + 18, ww - 12, 14, GUI_C_BLACK);
            gui_draw_text(fb, wx + 8, wy + 20, G.calc_disp, GUI_C_BR_GREEN, GUI_C_BLACK);
            const char *keys[] = { "7","8","9","/","4","5","6","*","1","2","3","-","0",".","=","+","C" };
            int kw = 36, kh = 14;
            for (int r = 0; r < 4; r++)
                for (int c = 0; c < 4; c++) {
                    int idx = r * 4 + c;
                    if (idx < 16)
                        gui_draw_button(fb, wx + 8 + c * 46, wy + 36 + r * 16, kw, kh, keys[idx], false, false);
                }
            gui_draw_button(fb, wx + 8, wy + 36 + 4 * 16, 80, kh, "C", false, false);
        } else if (app == APP_NOTEPAD) {
            int ww = 280, wh = 150;
            gui_draw_window(fb, wx, wy, ww, wh, "Notepad", top);
            gui_draw_text(fb, wx + 6, wy + 18, G.note_path, GUI_C_BLACK, GUI_C_LTGRAY);
            {
                int idx = 0, r = 0, c = 0;
                while (idx < G.note_len && r < NOTE_ROWS) {
                    char ch = G.note_buf[idx];
                    if (ch == '\n') {
                        r++;
                        c = 0;
                        idx++;
                        continue;
                    }
                    if (c >= NOTE_COLS) {
                        r++;
                        c = 0;
                        continue;
                    }
                    int py = wy + 30 + r * 8;
                    uint8_t bg = GUI_C_WHITE;
                    gui_draw_fill_rect(fb, wx + 8 + c * 8, py, 8, 8, bg);
                    char one[2] = { ch, '\0' };
                    gui_draw_text(fb, wx + 8 + c * 8, py, one, GUI_C_BLACK, bg);
                    idx++;
                    c++;
                }
                int cr, cc;
                note_index_to_rc(G.note_cursor, &cr, &cc);
                if (cr < NOTE_ROWS && top)
                    gui_draw_fill_rect(fb, wx + 8 + cc * 8, wy + 30 + cr * 8, 8, 8, GUI_C_YELLOW);
            }
            gui_draw_text(fb, wx + 6, wy + wh - 20, "^S save", GUI_C_DKGRAY, GUI_C_LTGRAY);
        } else if (app == APP_CLOCK) {
            int ww = 240, wh = 130;
            gui_draw_window(fb, wx, wy, ww, wh, "Clock", top);
            uint32_t now = timer_get_ms();
            char line[48];
            if (G.clk_mode == 0) {
                uint32_t tot = now / 1000;
                uint32_t h = (tot / 3600) % 99;
                uint32_t m = (tot / 60) % 60;
                uint32_t s = tot % 60;
                int p = 0;
                line[p++] = 'T';
                line[p++] = 'I';
                line[p++] = 'M';
                line[p++] = 'E';
                line[p++] = ' ';
                fmt_uint(line + p, h);
                p += (int)str_len(line + p);
                line[p++] = ':';
                if (m < 10) line[p++] = '0';
                fmt_uint(line + p, m);
                p += (int)str_len(line + p);
                line[p++] = ':';
                if (s < 10) line[p++] = '0';
                fmt_uint(line + p, s);
                p += (int)str_len(line + p);
                line[p] = '\0';
                gui_draw_text(fb, wx + 8, wy + 22, "Session clock (from boot)", GUI_C_BLACK, GUI_C_LTGRAY);
                gui_draw_text(fb, wx + 8, wy + 40, line, GUI_C_BLACK, GUI_C_LTGRAY);
            } else if (G.clk_mode == 1) {
                uint32_t el = G.clk_sw_running ? (now - G.clk_sw_start + G.clk_sw_acc) : G.clk_sw_acc;
                gui_draw_text(fb, wx + 8, wy + 22, "Stopwatch", GUI_C_BLACK, GUI_C_LTGRAY);
                fmt_uint(line, el);
                gui_draw_text(fb, wx + 8, wy + 38, line, GUI_C_BLACK, GUI_C_LTGRAY);
                gui_draw_text(fb, wx + 8, wy + 50, "ms", GUI_C_BLACK, GUI_C_LTGRAY);
                gui_draw_text(fb, wx + 8, wy + 68, "S start/stop  R reset", GUI_C_DKGRAY, GUI_C_LTGRAY);
            } else if (G.clk_mode == 2) {
                gui_draw_text(fb, wx + 8, wy + 22, "Timer (sec)", GUI_C_BLACK, GUI_C_LTGRAY);
                gui_draw_text(fb, wx + 8, wy + 36, G.clk_digits, GUI_C_BLACK, GUI_C_LTGRAY);
                if (G.clk_timer_active) {
                    int32_t left = (int32_t)(G.clk_timer_deadline - now);
                    if (left < 0) left = 0;
                    fmt_uint(line, (uint32_t)(left / 1000));
                    gui_draw_text(fb, wx + 8, wy + 52, line, GUI_C_BR_RED, GUI_C_LTGRAY);
                    if (left == 0) gui_draw_text(fb, wx + 8, wy + 66, "DONE", GUI_C_BR_RED, GUI_C_LTGRAY);
                }
                gui_draw_text(fb, wx + 8, wy + 88, "0-9 type  ENT=start", GUI_C_DKGRAY, GUI_C_LTGRAY);
            } else {
                gui_draw_text(fb, wx + 8, wy + 22, "Alarm (sec from now)", GUI_C_BLACK, GUI_C_LTGRAY);
                gui_draw_text(fb, wx + 8, wy + 36, G.clk_digits, GUI_C_BLACK, GUI_C_LTGRAY);
                if (G.clk_alarm_armed) {
                    int32_t left = (int32_t)(G.clk_alarm_at - now);
                    if (left <= 0) {
                        int flash = (int)((now / 400) % 2);
                        if (flash)
                            gui_draw_fill_rect(fb, wx + 4, wy + 50, ww - 8, 20, GUI_C_BR_RED);
                        gui_draw_text(fb, wx + 8, wy + 54, "ALARM!", GUI_C_WHITE, flash ? GUI_C_BR_RED : GUI_C_LTGRAY);
                    } else {
                        fmt_uint(line, (uint32_t)(left / 1000));
                        gui_draw_text(fb, wx + 8, wy + 54, line, GUI_C_BLACK, GUI_C_LTGRAY);
                    }
                }
                gui_draw_text(fb, wx + 8, wy + 88, "ENT=arm  0-9", GUI_C_DKGRAY, GUI_C_LTGRAY);
            }
            gui_draw_text(fb, wx + 8, wy + wh - 18, "1=clk 2=SW 3=TMR 4=ALM", GUI_C_DKGRAY, GUI_C_LTGRAY);
        } else if (app == APP_SNAKE) {
            int ww = GRID_W * CELL + 16, wh = GRID_H * CELL + 44;
            gui_draw_window(fb, wx, wy, ww, wh, "Snake", top);
            snake_draw(fb, wx + 8, wy + 18);
        }
    }
}

static void launch_from_pm(int row)
{
    if (row < 0 || row > 4) return;
    G.pm_sel = row;
    if (row == 0) {
        fm_refresh();
        stack_push(APP_FILE_MANAGER);
    } else if (row == 1) {
        calc_reset();
        stack_push(APP_CALCULATOR);
    } else if (row == 2) {
        if (!G.note_path[0]) {
            G.note_buf[0] = '\0';
            G.note_len = 0;
            G.note_cursor = 0;
            str_cpy(G.note_path, "/NOTE.TXT", sizeof(G.note_path));
        }
        stack_push(APP_NOTEPAD);
    } else if (row == 3) {
        G.clk_mode = 0;
        G.clk_digit_len = 0;
        G.clk_digits[0] = '\0';
        stack_push(APP_CLOCK);
    } else {
        snake_reset();
        stack_push(APP_SNAKE);
    }
}

static int calc_key_char(char key)
{
    if (key >= '0' && key <= '9') {
        size_t n = str_len(G.calc_disp);
        if (n + 1 < sizeof(G.calc_disp)) {
            if (n == 1 && G.calc_disp[0] == '0') {
                G.calc_disp[0] = key;
                G.calc_disp[1] = 0;
            } else {
                G.calc_disp[n++] = key;
                G.calc_disp[n] = 0;
            }
        }
        return 0;
    }
    if (key == '.') return 0;
    if (key == 'c' || key == 'C') {
        calc_reset();
        return 0;
    }
    if (key == '+' || key == '-' || key == '*' || key == '/') {
        calc_apply();
        G.calc_op = key;
        G.calc_disp[0] = '0';
        G.calc_disp[1] = 0;
        return 0;
    }
    if (key == '=' || key == '\n') {
        calc_apply();
        G.calc_op = 0;
        return 0;
    }
    return 0;
}

static void clk_digits_append(char d)
{
    if (G.clk_digit_len < (int)sizeof(G.clk_digits) - 1) {
        G.clk_digits[G.clk_digit_len++] = d;
        G.clk_digits[G.clk_digit_len] = '\0';
    }
}

static int handle_key(char c)
{
    if (c == 27) {
        if (stack_n <= 1)
            return 1;
        stack_pop();
        return 0;
    }
    if (c == '`') {
        stack_n = 0;
        stack_push(APP_PROGRAM_MANAGER);
        G.pm_sel = 0;
        return 0;
    }

    AppId t = stack_top();
    if (t == APP_FILE_MANAGER) {
        if (c == '\t') {
            if (G.fm_count > 0)
                G.fm_sel = (G.fm_sel + 1) % G.fm_count;
            return 0;
        }
        if (c == 'u' || c == 'U') {
            fm_parent();
            return 0;
        }
        if (c == '\n' || c == '\r') {
            fm_open_sel();
            return 0;
        }
        return 0;
    }
    if (t == APP_CALCULATOR) {
        calc_key_char(c);
        return 0;
    }
    if (t == APP_NOTEPAD) {
        if (c == 19) {
            if (G.note_path[0]) {
                if (!fs_exists(G.note_path))
                    fs_create(G.note_path);
                fs_write(G.note_path, G.note_buf, (size_t)G.note_len);
            }
            return 0;
        }
        if (c == '\b') {
            if (G.note_cursor > 0 && G.note_len > 0) {
                for (int i = G.note_cursor - 1; i < G.note_len; i++)
                    G.note_buf[i] = G.note_buf[i + 1];
                G.note_len--;
                G.note_cursor--;
                G.note_buf[G.note_len] = '\0';
            }
            return 0;
        }
        if (c == '\n' || c == '\r') {
            if (G.note_len + 1 < NOTE_BUF) {
                for (int i = G.note_len; i > G.note_cursor; i--)
                    G.note_buf[i] = G.note_buf[i - 1];
                G.note_buf[G.note_cursor] = '\n';
                G.note_len++;
                G.note_cursor++;
            }
            return 0;
        }
        if (c >= 32 && c < 127 && G.note_len + 1 < NOTE_BUF) {
            for (int i = G.note_len; i > G.note_cursor; i--)
                G.note_buf[i] = G.note_buf[i - 1];
            G.note_buf[G.note_cursor] = c;
            G.note_len++;
            G.note_cursor++;
            G.note_buf[G.note_len] = '\0';
        }
        return 0;
    }
    if (t == APP_CLOCK) {
        if (c == '1') {
            G.clk_mode = 0;
            return 0;
        }
        if (c == '2') {
            G.clk_mode = 1;
            return 0;
        }
        if (c == '3') {
            G.clk_mode = 2;
            G.clk_timer_active = 0;
            G.clk_digit_len = 0;
            G.clk_digits[0] = 0;
            return 0;
        }
        if (c == '4') {
            G.clk_mode = 3;
            G.clk_alarm_armed = 0;
            G.clk_digit_len = 0;
            G.clk_digits[0] = 0;
            return 0;
        }
        if (G.clk_mode == 1) {
            if (c == 's' || c == 'S') {
                uint32_t now = timer_get_ms();
                if (G.clk_sw_running) {
                    G.clk_sw_acc += now - G.clk_sw_start;
                    G.clk_sw_running = 0;
                } else {
                    G.clk_sw_start = now;
                    G.clk_sw_running = 1;
                }
            }
            if (c == 'r' || c == 'R') {
                G.clk_sw_acc = 0;
                G.clk_sw_running = 0;
            }
            return 0;
        }
        if (G.clk_mode == 2) {
            if (c >= '0' && c <= '9')
                clk_digits_append(c);
            if (c == '\b' && G.clk_digit_len > 0) {
                G.clk_digit_len--;
                G.clk_digits[G.clk_digit_len] = 0;
            }
            if (c == '\n' || c == '\r') {
                uint32_t sec = 0;
                for (int i = 0; G.clk_digits[i]; i++)
                    sec = sec * 10 + (uint32_t)(G.clk_digits[i] - '0');
                G.clk_timer_deadline = timer_get_ms() + sec * 1000;
                G.clk_timer_active = 1;
            }
            return 0;
        }
        if (G.clk_mode == 3) {
            if (c >= '0' && c <= '9')
                clk_digits_append(c);
            if (c == '\b' && G.clk_digit_len > 0) {
                G.clk_digit_len--;
                G.clk_digits[G.clk_digit_len] = 0;
            }
            if (c == '\n' || c == '\r') {
                uint32_t sec = 0;
                for (int i = 0; G.clk_digits[i]; i++)
                    sec = sec * 10 + (uint32_t)(G.clk_digits[i] - '0');
                G.clk_alarm_at = timer_get_ms() + sec * 1000;
                G.clk_alarm_armed = 1;
            }
            return 0;
        }
        return 0;
    }
    if (t == APP_SNAKE) {
        if (c == 'r' || c == 'R') {
            snake_reset();
            return 0;
        }
        if (c == 'w' || c == 'W')
            G.snake_dir = 0;
        else if (c == 'd' || c == 'D')
            G.snake_dir = 1;
        else if (c == 's' || c == 'S')
            G.snake_dir = 2;
        else if (c == 'a' || c == 'A')
            G.snake_dir = 3;
        return 0;
    }
    if (t == APP_PROGRAM_MANAGER) {
        if (c >= '1' && c <= '5')
            launch_from_pm((int)(c - '1'));
        return 0;
    }
    return 0;
}

static void handle_mouse_click(int px, int py)
{
    int z = stack_n - 1;
    if (z < 0) return;
    int wx, wy;
    win_offset(z, &wx, &wy);
    AppId t = stack[z];

    if (t == APP_PROGRAM_MANAGER) {
        int ww = 220, wh = 130;
        if (!gui_draw_hit(px, py, wx, wy, ww, wh)) return;
        if (!gui_draw_hit(px, py, wx + 8, wy + 18, ww - 16, 100)) return;
        int rel = py - (wy + 18);
        if (rel < 0) return;
        int row = rel / 20;
        if (row < 0 || row >= 5) return;
        launch_from_pm(row);
        return;
    }
    if (t == APP_CALCULATOR) {
        const char *keys = "789/456*123-0.=+";
        int kw = 36, kh = 14;
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++) {
                int bx = wx + 8 + c * 46;
                int by = wy + 36 + r * 16;
                if (gui_draw_hit(px, py, bx, by, kw, kh)) {
                    char k[2] = { keys[r * 4 + c], 0 };
                    calc_key_char(k[0]);
                    return;
                }
            }
        if (gui_draw_hit(px, py, wx + 8, wy + 36 + 4 * 16, 80, kh))
            calc_key_char('C');
        return;
    }
}

void gui_run(void)
{
    video_mode13_enter();
    mouse_init();
    mx = 160;
    my = 100;
    prev_btn = 0;
    stack_n = 0;
    stack_push(APP_PROGRAM_MANAGER);
    G.pm_sel = 0;
    fm_refresh();
    calc_reset();
    G.note_buf[0] = '\0';
    G.note_len = 0;
    G.note_cursor = 0;
    G.note_path[0] = '\0';
    G.clk_mode = 0;
    G.clk_sw_running = 0;
    G.clk_sw_acc = 0;
    G.clk_timer_active = 0;
    G.clk_digit_len = 0;
    G.clk_digits[0] = '\0';
    G.clk_alarm_armed = 0;
    snake_reset();

    uint8_t *fb = video_mode13_framebuffer();
    gui_draw_apply_palette();

    for (;;) {
        if (stack_top() == APP_SNAKE)
            snake_tick(fb, 0, 0);

        int dx, dy, btn;
        mouse_poll(&dx, &dy, &btn);
        mx += dx;
        my += dy;
        if (mx < 0) mx = 0;
        if (mx >= GUI_FB_W) mx = GUI_FB_W - 1;
        if (my < 0) my = 0;
        if (my >= GUI_FB_H) my = GUI_FB_H - 1;

        char key;
        while ((key = keyboard_getchar()) != 0) {
            if (handle_key(key)) {
                video_mode13_leave();
                return;
            }
        }

        if ((btn & MOUSE_BTN_LEFT) && !(prev_btn & MOUSE_BTN_LEFT))
            handle_mouse_click(mx, my);
        prev_btn = btn;

        draw_desktop(fb);
        __asm__ volatile("hlt");
    }
}
</think>
Fixing errors in `gui.c`: correcting hit-testing, notepad rendering, and removing the invalid function.

<｜tool▁calls▁begin｜><｜tool▁call▁begin｜>
Read