/**
 * Minimal in-memory filesystem.
 * Single root; supports mkdir, create, read, write, list, chdir.
 * No persistence (resets on reboot).
 */

#include <kernel/fs.h>
#include <kernel/types.h>

static struct fs_node files[FS_MAX_FILES];
static struct fs_node dirs[FS_MAX_DIRS];
static size_t nfiles;
static size_t ndirs;
static size_t cwd;  /* index into dirs (0 = root) */

static size_t root_dir(void) { return 0; }

static void normalize_path(const char *path, char *out, size_t max_len)
{
    size_t i = 0, j = 0;
    if (path[0] == '/') { out[j++] = '/'; i = 1; }
    while (path[i] && j < max_len - 1) {
        if (path[i] == '.' && path[i+1] == '.' && (path[i+2] == '/' || path[i+2] == '\0')) {
            while (j > 0 && out[j-1] != '/') j--;
            if (j > 0) j--;
            i += 2;
            if (path[i] == '/') i++;
            continue;
        }
        if (path[i] == '.' && (path[i+1] == '/' || path[i+1] == '\0')) {
            i++; if (path[i] == '/') i++;
            continue;
        }
        if (path[i] == '/') {
            if (j > 0 && out[j-1] != '/') out[j++] = '/';
            i++;
            continue;
        }
        out[j++] = path[i++];
    }
    if (j > 1 && out[j-1] == '/') j--;
    out[j] = '\0';
}

void fs_init(void)
{
    nfiles = 0;
    ndirs = 0;
    cwd = root_dir();
    dirs[0].type = FS_DIR;
    dirs[0].name[0] = '/';
    dirs[0].name[1] = '\0';
    dirs[0].parent = 0;
    ndirs = 1;
}

static size_t dir_by_name(size_t parent, const char *name)
{
    for (size_t i = 0; i < ndirs; i++)
        if (dirs[i].parent == parent) {
            size_t k = 0;
            while (name[k] && dirs[i].name[k] && name[k] == dirs[i].name[k]) k++;
            if (!name[k] && !dirs[i].name[k]) return i;
        }
    return (size_t)-1;
}

static size_t file_by_name(size_t parent, const char *name)
{
    for (size_t i = 0; i < nfiles; i++)
        if (files[i].parent == parent) {
            size_t k = 0;
            while (name[k] && files[i].name[k] && name[k] == files[i].name[k]) k++;
            if (!name[k] && !files[i].name[k]) return i;
        }
    return (size_t)-1;
}

static int path_to_dir(const char *path, size_t *out_dir)
{
    char norm[FS_PATH_MAX];
    normalize_path(path, norm, sizeof(norm));
    if (norm[0] != '/') return -1;
    size_t d = root_dir();
    if (norm[1] == '\0') { *out_dir = d; return 0; }
    const char *p = norm + 1;
    while (*p) {
        char part[FS_NAME_MAX];
        size_t i = 0;
        while (*p && *p != '/' && i < FS_NAME_MAX - 1) part[i++] = *p++;
        part[i] = '\0';
        if (*p == '/') p++;
        if (!part[0]) break;
        size_t next = dir_by_name(d, part);
        if (next == (size_t)-1) return -1;
        d = next;
    }
    *out_dir = d;
    return 0;
}

int fs_mkdir(const char *path)
{
    char norm[FS_PATH_MAX];
    normalize_path(path, norm, sizeof(norm));
    if (ndirs >= FS_MAX_DIRS) return -1;
    const char *last = norm;
    const char *p = norm + 1;
    while (*p) { if (*p == '/') last = p + 1; p++; }
    if (!*last) return -1;
    char parent_path[FS_PATH_MAX];
    size_t full_plen = (size_t)(last - norm);
    size_t plen = (full_plen >= 2) ? full_plen - 2 : 0;
    if (plen >= sizeof(parent_path)) return -1;
    for (size_t i = 0; i < plen; i++) parent_path[i] = norm[i];
    parent_path[plen] = '\0';
    size_t parent;
    if (plen == 0) parent = root_dir();
    else if (path_to_dir(parent_path, &parent) != 0) return -1;
    if (dir_by_name(parent, last) != (size_t)-1) return -1; /* already exists */
    dirs[ndirs].type = FS_DIR;
    size_t nlen = 0;
    while (last[nlen] && last[nlen] != '/' && nlen < FS_NAME_MAX - 1)
        dirs[ndirs].name[nlen] = last[nlen], nlen++;
    dirs[ndirs].name[nlen] = '\0';
    dirs[ndirs].parent = parent;
    ndirs++;
    return 0;
}

int fs_create(const char *path)
{
    char norm[FS_PATH_MAX];
    normalize_path(path, norm, sizeof(norm));
    if (nfiles >= FS_MAX_FILES) return -1;
    const char *last = norm[0] == '/' ? norm + 1 : norm;
    const char *p = last;
    while (*p) { if (*p == '/') last = p + 1; p++; }
    if (!*last) return -1;
    char parent_path[FS_PATH_MAX];
    size_t full_plen = (size_t)(last - norm);
    size_t plen = (full_plen >= 2) ? full_plen - 2 : 0;
    if (plen >= sizeof(parent_path)) return -1;
    for (size_t i = 0; i < plen; i++) parent_path[i] = norm[i];
    parent_path[plen] = '\0';
    size_t parent;
    if (plen == 0) parent = root_dir();
    else if (path_to_dir(parent_path, &parent) != 0) return -1;
    if (file_by_name(parent, last) != (size_t)-1) return -1;
    files[nfiles].type = FS_FILE;
    size_t nlen = 0;
    while (last[nlen] && last[nlen] != '/' && nlen < FS_NAME_MAX - 1)
        files[nfiles].name[nlen] = last[nlen], nlen++;
    files[nfiles].name[nlen] = '\0';
    files[nfiles].parent = parent;
    files[nfiles].size = 0;
    files[nfiles].data = NULL;
    nfiles++;
    return 0;
}

int fs_write(const char *path, const char *content, size_t len)
{
    char norm[FS_PATH_MAX];
    normalize_path(path, norm, sizeof(norm));
    const char *last = norm[0] == '/' ? norm + 1 : norm;
    const char *p = last;
    while (*p) { if (*p == '/') last = p + 1; p++; }
    size_t parent;
    size_t full_plen = (size_t)(last - norm);
    size_t plen = (full_plen >= 2) ? full_plen - 2 : 0; /* parent path without trailing / */
    char parent_path[FS_PATH_MAX];
    for (size_t i = 0; i < plen && i < sizeof(parent_path)-1; i++) parent_path[i] = norm[i];
    parent_path[plen] = '\0';
    if (plen == 0) parent = root_dir();
    else if (path_to_dir(parent_path, &parent) != 0) return -1;
    size_t fi = file_by_name(parent, last);
    if (fi == (size_t)-1) return -1;
    /* In-memory: we don't have malloc yet, so use a static buffer per file (simplified) */
    if (len > FS_FILE_BUF) len = FS_FILE_BUF;
    static char file_bufs[FS_MAX_FILES][FS_FILE_BUF];
    for (size_t i = 0; i < len; i++) file_bufs[fi][i] = content[i];
    files[fi].data = file_bufs[fi];
    files[fi].size = len;
    return 0;
}

static int resolve_file_path(const char *path, size_t *out_parent, const char **out_name)
{
    char norm[FS_PATH_MAX];
    char abs[FS_PATH_MAX];
    if (path[0] == '/') {
        normalize_path(path, norm, sizeof(norm));
    } else {
        fs_get_cwd(abs, sizeof(abs));
        size_t i = 0;
        while (abs[i]) i++;
        if (i && abs[i-1] != '/') abs[i++] = '/';
        size_t j = 0;
        while (path[j] && i < sizeof(abs)-1) abs[i++] = path[j++];
        abs[i] = '\0';
        normalize_path(abs, norm, sizeof(norm));
    }
    const char *last = norm[0] == '/' ? norm + 1 : norm;
    const char *p = last;
    while (*p) { if (*p == '/') last = p + 1; p++; }
    size_t full_plen = (size_t)(last - norm);
    size_t plen = (full_plen >= 2) ? full_plen - 2 : 0;
    char parent_path[FS_PATH_MAX];
    for (size_t i = 0; i < plen && i < sizeof(parent_path)-1; i++) parent_path[i] = norm[i];
    parent_path[plen] = '\0';
    size_t parent;
    if (plen == 0) parent = root_dir();
    else if (path_to_dir(parent_path, &parent) != 0) return -1;
    *out_parent = parent;
    *out_name = last;
    return 0;
}

int fs_read(const char *path, char *buf, size_t max_len)
{
    size_t parent;
    const char *last;
    if (resolve_file_path(path, &parent, &last) != 0) return -1;
    size_t fi = file_by_name(parent, last);
    if (fi == (size_t)-1) return -1;
    if (!files[fi].data) { buf[0] = '\0'; return 0; }
    size_t n = files[fi].size;
    if (n >= max_len) n = max_len - 1;
    for (size_t i = 0; i < n; i++) buf[i] = files[fi].data[i];
    buf[n] = '\0';
    return (int)n;
}

int fs_list(const char *path, char *buf, size_t max_len)
{
    size_t d;
    if (path && path[0]) {
        if (path_to_dir(path, &d) != 0) return -1;
    } else
        d = cwd;
    size_t j = 0;
    for (size_t i = 0; i < ndirs && j < max_len - 1; i++)
        if (dirs[i].parent == d) {
            size_t k = 0;
            while (dirs[i].name[k] && j < max_len - 1) buf[j++] = dirs[i].name[k++];
            buf[j++] = '/';
            if (j < max_len - 1) buf[j++] = ' ';
        }
    for (size_t i = 0; i < nfiles && j < max_len - 1; i++)
        if (files[i].parent == d) {
            size_t k = 0;
            while (files[i].name[k] && j < max_len - 1) buf[j++] = files[i].name[k++];
            if (j < max_len - 1) buf[j++] = ' ';
        }
    if (j > 0 && buf[j-1] == ' ') j--;
    buf[j] = '\0';
    return 0;
}

int fs_chdir(const char *path)
{
    size_t d;
    if (path_to_dir(path, &d) != 0) return -1;
    cwd = d;
    return 0;
}

void fs_get_cwd(char *buf, size_t max_len)
{
    if (cwd == root_dir()) { buf[0] = '/'; buf[1] = '\0'; return; }
    /* Walk parent chain and build path */
    size_t stack[FS_MAX_DIRS], sp = 0;
    size_t cur = cwd;
    while (cur != root_dir()) {
        stack[sp++] = cur;
        cur = dirs[cur].parent;
    }
    buf[0] = '/';
    size_t j = 1;
    while (sp > 0 && j < max_len - 1) {
        size_t i = stack[--sp];
        size_t k = 0;
        while (dirs[i].name[k] && j < max_len - 1) buf[j++] = dirs[i].name[k++];
        if (sp > 0 && j < max_len - 1) buf[j++] = '/';
    }
    buf[j] = '\0';
}

int fs_exists(const char *path)
{
    size_t d;
    if (path_to_dir(path, &d) == 0) return 1;
    char norm[FS_PATH_MAX];
    normalize_path(path, norm, sizeof(norm));
    const char *last = norm + 1;
    const char *p = last;
    while (*p) { if (*p == '/') last = p + 1; p++; }
    char parent_path[FS_PATH_MAX];
    size_t plen = (size_t)(last - norm - 1);
    for (size_t i = 0; i < plen && i < sizeof(parent_path)-1; i++) parent_path[i] = norm[i];
    parent_path[plen] = '\0';
    if (path_to_dir(plen ? parent_path : "/", &d) != 0) return 0;
    if (file_by_name(d, last) != (size_t)-1) return 1;
    return 0;
}
