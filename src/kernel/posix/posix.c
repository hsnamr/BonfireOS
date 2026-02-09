/**
 * POSIX compatibility layer: fd table, open/read/write/close/lseek/getcwd/chdir/mkdir/stat
 * backed by in-memory fs and console (VGA/keyboard).
 */

#include <kernel/posix.h>
#include <kernel/fs.h>
#include <kernel/vga.h>
#include <kernel/keyboard.h>
#include <kernel/types.h>

int errno;

enum fd_type { FD_NONE, FD_CONSOLE_IN, FD_CONSOLE_OUT, FD_IMEM };

struct fd_entry {
    enum fd_type type;
    char path[FS_PATH_MAX];
    size_t offset;
};

static struct fd_entry fd_table[OPEN_MAX];
static bool fd_init;

static void fd_table_init(void)
{
    if (fd_init) return;
    fd_init = true;
    for (int i = 0; i < OPEN_MAX; i++) fd_table[i].type = FD_NONE;
    fd_table[STDIN_FILENO].type = FD_CONSOLE_IN;
    fd_table[STDOUT_FILENO].type = FD_CONSOLE_OUT;
    fd_table[STDERR_FILENO].type = FD_CONSOLE_OUT;
}

static int alloc_fd(void)
{
    for (int i = STDERR_FILENO + 1; i < OPEN_MAX; i++)
        if (fd_table[i].type == FD_NONE) return i;
    return -1;
}

int open(const char *path, int flags)
{
    (void)flags;
    fd_table_init();
    int fd = alloc_fd();
    if (fd < 0) { errno = -24; return -1; }
    size_t i = 0;
    while (path[i] && i < FS_PATH_MAX - 1) fd_table[fd].path[i] = path[i], i++;
    fd_table[fd].path[i] = '\0';
    fd_table[fd].type = FD_IMEM;
    fd_table[fd].offset = 0;
    if (!fs_exists(path)) {
        if (fs_create(path) != 0) { fd_table[fd].type = FD_NONE; errno = -2; return -1; }
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count)
{
    fd_table_init();
    if (fd < 0 || fd >= OPEN_MAX || fd_table[fd].type == FD_NONE) { errno = -9; return -1; }
    if (fd_table[fd].type == FD_CONSOLE_IN) {
        size_t n = 0;
        char *p = (char *)buf;
        while (n < count) {
            char c = keyboard_getchar();
            if (!c) break;
            p[n++] = c;
        }
        return (ssize_t)n;
    }
    if (fd_table[fd].type == FD_IMEM) {
        int r = fs_read(fd_table[fd].path, (char *)buf, count);
        if (r < 0) { errno = -2; return -1; }
        fd_table[fd].offset += (size_t)r;
        return (ssize_t)r;
    }
    errno = -9;
    return -1;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    fd_table_init();
    if (fd < 0 || fd >= OPEN_MAX || fd_table[fd].type == FD_NONE) { errno = -9; return -1; }
    if (fd_table[fd].type == FD_CONSOLE_OUT) {
        const char *p = (const char *)buf;
        for (size_t i = 0; i < count; i++) vga_putchar(p[i]);
        return (ssize_t)count;
    }
    if (fd_table[fd].type == FD_IMEM) {
        if (fs_write(fd_table[fd].path, (const char *)buf, count) != 0) { errno = -2; return -1; }
        fd_table[fd].offset += count;
        return (ssize_t)count;
    }
    errno = -9;
    return -1;
}

int close(int fd)
{
    fd_table_init();
    if (fd < 0 || fd >= OPEN_MAX) { errno = -9; return -1; }
    if (fd <= STDERR_FILENO) return 0;
    fd_table[fd].type = FD_NONE;
    return 0;
}

off_t lseek(int fd, off_t offset, int whence)
{
    fd_table_init();
    if (fd < 0 || fd >= OPEN_MAX || fd_table[fd].type != FD_IMEM) { errno = -9; return -1; }
    if (whence == SEEK_SET) fd_table[fd].offset = (size_t)offset;
    else if (whence == SEEK_CUR) fd_table[fd].offset += (size_t)offset;
    else { errno = -22; return -1; }
    return (off_t)fd_table[fd].offset;
}

int getcwd(char *buf, size_t size)
{
    fs_get_cwd(buf, size);
    return 0;
}

int chdir(const char *path)
{
    return fs_chdir(path) == 0 ? 0 : (errno = -2, -1);
}

int mkdir(const char *path)
{
    return fs_mkdir(path) == 0 ? 0 : (errno = -2, -1);
}

int stat(const char *path, struct stat *st)
{
    if (!st) { errno = -14; return -1; }
    st->st_mode = S_IFREG;
    st->st_size = 0;
    if (fs_exists(path)) {
        char tmp[FS_FILE_BUF];
        int n = fs_read(path, tmp, sizeof(tmp));
        if (n >= 0) st->st_size = (uint32_t)n;
    }
    return 0;
}
