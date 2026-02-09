#ifndef BONFIRE_POSIX_H
#define BONFIRE_POSIX_H

#include <kernel/types.h>

typedef int64_t off_t;

/* POSIX-like API for kernel and future userland. */

#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   0x0040
#define O_TRUNC   0x0200

#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define OPEN_MAX  16

extern int errno;

int open(const char *path, int flags);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int getcwd(char *buf, size_t size);
int chdir(const char *path);
int mkdir(const char *path);
int stat(const char *path, struct stat *st);

struct stat {
    uint32_t st_mode;
    uint32_t st_size;
};

#define S_IFREG  0100000
#define S_IFDIR  0040000

#endif /* BONFIRE_POSIX_H */
