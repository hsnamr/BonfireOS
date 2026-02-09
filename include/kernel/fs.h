#ifndef BONFIRE_FS_H
#define BONFIRE_FS_H

#include <kernel/types.h>

#define FS_NAME_MAX  32
#define FS_PATH_MAX  128
#define FS_MAX_FILES 64
#define FS_MAX_DIRS  32
#define FS_FILE_BUF  4096

enum fs_node_type { FS_FILE, FS_DIR };

struct fs_node {
    enum fs_node_type type;
    char name[FS_NAME_MAX];
    size_t parent;   /* index into dirs[] or 0 for root */
    size_t size;     /* file size in bytes */
    char *data;     /* file content (files only) */
};

/* In-memory minimal FS: single root dir, fixed max files/dirs */
void fs_init(void);
int fs_mkdir(const char *path);
int fs_create(const char *path);
int fs_write(const char *path, const char *content, size_t len);
int fs_read(const char *path, char *buf, size_t max_len);
int fs_list(const char *path, char *buf, size_t max_len);
int fs_chdir(const char *path);
void fs_get_cwd(char *buf, size_t max_len);
int fs_exists(const char *path);

#endif /* BONFIRE_FS_H */
