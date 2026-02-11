/**
 * Simple shell: read line, expand aliases, dispatch commands.
 * Commands: help, clear, echo, ls, cd, mkdir, cat, edit, alias.
 */

#include <kernel/shell.h>
#include <kernel/alias.h>
#include <kernel/fs.h>
#include <kernel/fat.h>
#include <kernel/vga.h>
#include <kernel/keyboard.h>
#include <kernel/doom_host.h>
#include <kernel/mouse.h>
#include <kernel/types.h>

#define LINE_MAX 256

static char line_buf[LINE_MAX];
static char expanded_buf[LINE_MAX];
static size_t line_len;

static void skip_spaces(const char **p) { while (**p == ' ') (*p)++; }
static void next_arg(const char **p, char *buf, size_t max_len)
{
    skip_spaces(p);
    size_t i = 0;
    while (**p && **p != ' ' && i < max_len - 1) buf[i++] = *(*p)++;
    buf[i] = '\0';
}

static void cmd_help(void)
{
    vga_puts("Commands: help clear echo ls cd mkdir cat edit alias fatcat DOOM\n");
}

static void cmd_clear(void)
{
    vga_clear();
}

static void cmd_echo(const char *args)
{
    skip_spaces(&args);
    vga_puts(args);
    vga_putchar('\n');
}

static void cmd_ls(const char *args)
{
    char path[FS_PATH_MAX];
    next_arg(&args, path, sizeof(path));
    char out[512];
    if (fs_list(path[0] ? path : NULL, out, sizeof(out)) != 0) {
        vga_puts("ls: cannot list\n");
        return;
    }
    vga_puts(out);
    vga_putchar('\n');
}

static void cmd_cd(const char *args)
{
    char path[FS_PATH_MAX];
    next_arg(&args, path, sizeof(path));
    if (!path[0]) { vga_puts("cd: missing path\n"); return; }
    if (fs_chdir(path) != 0) {
        vga_puts("cd: no such directory\n");
        return;
    }
}

static void cmd_mkdir(const char *args)
{
    char path[FS_PATH_MAX];
    next_arg(&args, path, sizeof(path));
    if (!path[0]) { vga_puts("mkdir: missing path\n"); return; }
    if (fs_mkdir(path) != 0) {
        vga_puts("mkdir: failed\n");
        return;
    }
}

static void cmd_cat(const char *args)
{
    char path[FS_PATH_MAX];
    next_arg(&args, path, sizeof(path));
    if (!path[0]) { vga_puts("cat: missing file\n"); return; }
    char buf[FS_FILE_BUF];
    int n = fs_read(path, buf, sizeof(buf));
    if (n < 0) {
        vga_puts("cat: cannot read file\n");
        return;
    }
    vga_puts(buf);
    if (n > 0 && buf[n-1] != '\n') vga_putchar('\n');
}

/* Read file from FAT root (8.3 name, e.g. FILE.TXT or FILE) */
static void cmd_fatcat(const char *args)
{
    char name[12];
    next_arg(&args, name, sizeof(name));
    if (!name[0]) { vga_puts("fatcat: missing 8.3 filename\n"); return; }
    char name_83[11];
    size_t i = 0, j = 0;
    for (; name[i] && name[i] != '.' && j < 8; i++) name_83[j++] = name[i];
    while (j < 8) name_83[j++] = ' ';
    if (name[i] == '.') i++;
    for (; name[i] && j < 11; i++) name_83[j++] = name[i];
    while (j < 11) name_83[j++] = ' ';
    uint32_t cluster, size;
    if (fat_find_root(name_83, &cluster, &size) != 0) {
        vga_puts("fatcat: file not found on disk\n");
        return;
    }
    char buf[FS_FILE_BUF];
    if (size > sizeof(buf)) size = sizeof(buf);
    int n = fat_read_file(cluster, size, buf);
    if (n > 0) {
        for (int k = 0; k < n; k++) vga_putchar(buf[k]);
        if (n > 0 && buf[n-1] != '\n') vga_putchar('\n');
    }
}

static void cmd_edit(const char *args)
{
    char path[FS_PATH_MAX];
    next_arg(&args, path, sizeof(path));
    if (!path[0]) { vga_puts("edit: missing file\n"); return; }
    if (!fs_exists(path)) {
        if (fs_create(path) != 0) { vga_puts("edit: cannot create file\n"); return; }
    }
    vga_puts("Enter content (single line), then press Enter:\n");
    char content[FS_FILE_BUF];
    size_t i = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (!c) { __asm__ volatile ("hlt"); continue; }
        if (c == '\n') break;
        if (i < sizeof(content) - 1) content[i++] = c;
        vga_putchar(c);
    }
    content[i] = '\0';
    vga_putchar('\n');
    if (fs_write(path, content, i) != 0) vga_puts("edit: write failed\n");
    else vga_puts("Saved.\n");
}

static void cmd_alias(const char *args)
{
    char name[ALIAS_NAME_MAX], value[ALIAS_VALUE_MAX];
    next_arg(&args, name, sizeof(name));
    if (!name[0]) {
        vga_puts("alias: usage alias <name> <value>\n");
        return;
    }
    skip_spaces(&args);
    size_t i = 0;
    while (args[i] && i < ALIAS_VALUE_MAX - 1) value[i] = args[i], i++;
    value[i] = '\0';
    if (alias_set(name, value) != 0) vga_puts("alias: failed\n");
    else vga_puts("ok\n");
}

static void cmd_doom(const char *args)
{
    (void)args;
    doom_video_enter();
    mouse_init();
    doom_input_clear();
    static char *argv[] = { "DOOM", NULL };
    int ret = doom_main(1, argv);
    doom_video_leave();
    if (ret != 0)
        vga_puts("DOOM not available (link a DOOM port to provide doom_main).\n");
}

static void run_command(char *line)
{
    alias_parse_and_expand(line, expanded_buf, sizeof(expanded_buf));
    const char *p = expanded_buf;
    skip_spaces(&p);
    if (!*p) return;
    char cmd[32];
    next_arg(&p, cmd, sizeof(cmd));
    if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p' && !cmd[4]) { cmd_help(); return; }
    if (cmd[0] == 'c' && cmd[1] == 'l' && cmd[2] == 'e' && cmd[3] == 'a' && cmd[4] == 'r' && !cmd[5]) { cmd_clear(); return; }
    if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' && !cmd[4]) { cmd_echo(p); return; }
    if (cmd[0] == 'l' && cmd[1] == 's' && !cmd[2]) { cmd_ls(p); return; }
    if (cmd[0] == 'c' && cmd[1] == 'd' && !cmd[2]) { cmd_cd(p); return; }
    if (cmd[0] == 'm' && cmd[1] == 'k' && cmd[2] == 'd' && cmd[3] == 'i' && cmd[4] == 'r' && !cmd[5]) { cmd_mkdir(p); return; }
    if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't' && !cmd[3]) { cmd_cat(p); return; }
    if (cmd[0] == 'e' && cmd[1] == 'd' && cmd[2] == 'i' && cmd[3] == 't' && !cmd[4]) { cmd_edit(p); return; }
    if (cmd[0] == 'a' && cmd[1] == 'l' && cmd[2] == 'i' && cmd[3] == 'a' && cmd[4] == 's' && !cmd[5]) { cmd_alias(p); return; }
    if (cmd[0] == 'f' && cmd[1] == 'a' && cmd[2] == 't' && cmd[3] == 'c' && cmd[4] == 'a' && cmd[5] == 't' && !cmd[6]) { cmd_fatcat(p); return; }
    if (cmd[0] == 'D' && cmd[1] == 'O' && cmd[2] == 'O' && cmd[3] == 'M' && !cmd[4]) { cmd_doom(p); return; }
    vga_puts("Unknown command. Type 'help' for list.\n");
}

void shell_init(void)
{
    line_len = 0;
    fs_init();
}

void shell_run(void)
{
    for (;;) {
        char c = keyboard_getchar();
        if (!c) {
            __asm__ volatile ("hlt");
            continue;
        }
        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            line_buf[line_len] = '\0';
            run_command(line_buf);
            line_len = 0;
            vga_puts("> ");
            continue;
        }
        if (c == '\b') {
            if (line_len) {
                line_len--;
                vga_putchar('\b');
                vga_putchar(' ');
                vga_putchar('\b');
            }
            continue;
        }
        if (line_len < LINE_MAX - 1) {
            line_buf[line_len++] = c;
            vga_putchar(c);
        }
    }
}
