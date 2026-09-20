#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

extern int  ramfs_resolve(const char *path);
extern bool ramfs_is_dir(int idx);
extern bool ramfs_valid(int idx);
extern const char *ramfs_name(int idx);
extern int  ramfs_parent(int idx);
extern int  ramfs_create(int parent_idx, const char *name, bool is_dir);
extern int  ramfs_next_child(int dir_idx, int *cursor);
extern bool ramfs_delete(int idx);
extern bool ramfs_rename(int idx, int new_parent_idx, const char *new_name);
extern int  ramfs_read(int idx, int offset, void *out, int max_len);
extern int  ramfs_write(int idx, int offset, const void *in, int len);
extern int  ramfs_truncate(int idx);
extern int  ramfs_size(int idx);

static char g_cwd[512] = "/";

static void to_absolute(const char *path, char *out, size_t outsz) {
    if (path[0] == '/') {
        snprintf(out, outsz, "%s", path);
    } else {
        if (strcmp(g_cwd, "/") == 0) snprintf(out, outsz, "/%s", path);
        else snprintf(out, outsz, "%s/%s", g_cwd, path);
    }
}

static void split_path(const char *abspath, char *parent_out, size_t parent_sz,
                        char *leaf_out, size_t leaf_sz) {
    const char *slash = strrchr(abspath, '/');
    if (!slash || slash == abspath) {
        snprintf(parent_out, parent_sz, "/");
        snprintf(leaf_out, leaf_sz, "%s", slash ? slash + 1 : abspath);
        return;
    }
    size_t plen = (size_t)(slash - abspath);
    if (plen >= parent_sz) plen = parent_sz - 1;
    memcpy(parent_out, abspath, plen);
    parent_out[plen] = '\0';
    snprintf(leaf_out, leaf_sz, "%s", slash + 1);
}

typedef struct { int dir_idx; int cursor; } MyDIR;

DIR *opendir(const char *name) {
    char abs[512];
    to_absolute(name, abs, sizeof(abs));
    int idx = ramfs_resolve(abs);
    if (!ramfs_valid(idx) || !ramfs_is_dir(idx)) return NULL;
    MyDIR *d = (MyDIR *)malloc(sizeof(MyDIR));
    if (!d) return NULL;
    d->dir_idx = idx;
    d->cursor = 0;
    return (DIR *)d;
}

struct dirent *readdir(DIR *dirp) {
    static struct dirent de;
    MyDIR *d = (MyDIR *)dirp;
    if (!d) return NULL;
    int child = ramfs_next_child(d->dir_idx, &d->cursor);
    if (child < 0) return NULL;
    memset(&de, 0, sizeof(de));
    snprintf(de.d_name, sizeof(de.d_name), "%s", ramfs_name(child));
    return &de;
}

int closedir(DIR *dirp) {
    free(dirp);
    return 0;
}

int stat(const char *path, struct stat *buf) {
    char abs[512];
    to_absolute(path, abs, sizeof(abs));
    int idx = ramfs_resolve(abs);
    if (!ramfs_valid(idx)) return -1;
    memset(buf, 0, sizeof(*buf));
    buf->st_mode = ramfs_is_dir(idx) ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    buf->st_size = ramfs_size(idx);
    return 0;
}

int mkdir(const char *path, mode_t mode) {
    (void)mode;
    char abs[512], parent[512], leaf[128];
    to_absolute(path, abs, sizeof(abs));
    split_path(abs, parent, sizeof(parent), leaf, sizeof(leaf));
    int pidx = ramfs_resolve(parent);
    if (!ramfs_valid(pidx) || !ramfs_is_dir(pidx)) return -1;
    int idx = ramfs_create(pidx, leaf, true);
    return idx >= 0 ? 0 : -1;
}

char *dirname(char *path) {
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
    char *slash = strrchr(path, '/');
    if (!slash) return dot;
    if (slash == path) { path[1] = '\0'; return path; }
    *slash = '\0';
    return path;
}

char *getcwd(char *buf, size_t size) {
    if (strlen(g_cwd) >= size) return NULL;
    strcpy(buf, g_cwd);
    return buf;
}

int chdir(const char *path) {
    char abs[512];
    to_absolute(path, abs, sizeof(abs));
    int idx = ramfs_resolve(abs);
    if (!ramfs_valid(idx) || !ramfs_is_dir(idx)) return -1;
    snprintf(g_cwd, sizeof(g_cwd), "%s", abs);
    if (g_cwd[0] == '\0') strcpy(g_cwd, "/");
    return 0;
}

char *getenv(const char *name) {
    if (strcmp(name, "HOME") == 0) return "/";
    return NULL;
}

typedef struct {
    int  node_idx;
    int  pos;
    bool is_popen;
    char *pop_buf;
    int   pop_len;
    int   pop_pos;
} MyFile;

FILE *fopen(const char *path, const char *mode) {
    char abs[512];
    to_absolute(path, abs, sizeof(abs));
    int idx = ramfs_resolve(abs);

    bool creating = strchr(mode, 'w') || strchr(mode, 'a');
    if (idx < 0) {
        if (!creating) return NULL;
        char parent[512], leaf[128];
        split_path(abs, parent, sizeof(parent), leaf, sizeof(leaf));
        int pidx = ramfs_resolve(parent);
        if (!ramfs_valid(pidx) || !ramfs_is_dir(pidx)) return NULL;
        idx = ramfs_create(pidx, leaf, false);
        if (idx < 0) return NULL;
    }
    if (ramfs_is_dir(idx)) return NULL;
    if (strchr(mode, 'w')) ramfs_truncate(idx);

    MyFile *f = (MyFile *)malloc(sizeof(MyFile));
    if (!f) return NULL;
    f->node_idx = idx;
    f->pos = strchr(mode, 'a') ? ramfs_size(idx) : 0;
    f->is_popen = false;
    f->pop_buf = NULL;
    return (FILE *)f;
}

int fclose(FILE *stream) {
    free(stream);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    MyFile *f = (MyFile *)stream;
    int want = (int)(size * nmemb);
    int got = ramfs_read(f->node_idx, f->pos, ptr, want);
    if (got < 0) return 0;
    f->pos += got;
    return (size_t)(got / (int)size);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    MyFile *f = (MyFile *)stream;
    int want = (int)(size * nmemb);
    int wrote = ramfs_write(f->node_idx, f->pos, ptr, want);
    if (wrote < 0) return 0;
    f->pos += wrote;
    return (size_t)(wrote / (int)size);
}

char *fgets(char *s, int size, FILE *stream) {
    MyFile *f = (MyFile *)stream;
    if (size <= 0) return NULL;

    if (f->is_popen) {
        if (f->pop_pos >= f->pop_len) return NULL;
        int i = 0;
        while (i < size - 1 && f->pop_pos < f->pop_len) {
            char c = f->pop_buf[f->pop_pos++];
            s[i++] = c;
            if (c == '\n') break;
        }
        s[i] = '\0';
        return i > 0 ? s : NULL;
    }

    int i = 0;
    char c;
    while (i < size - 1) {
        int got = ramfs_read(f->node_idx, f->pos, &c, 1);
        if (got <= 0) break;
        f->pos++;
        s[i++] = c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return i > 0 ? s : NULL;
}

int remove(const char *path) {
    char abs[512];
    to_absolute(path, abs, sizeof(abs));
    int idx = ramfs_resolve(abs);
    if (!ramfs_valid(idx)) return -1;
    return ramfs_delete(idx) ? 0 : -1;
}

int rename(const char *old, const char *newp) {
    char oldabs[512], newabs[512], parent[512], leaf[128];
    to_absolute(old, oldabs, sizeof(oldabs));
    to_absolute(newp, newabs, sizeof(newabs));
    int idx = ramfs_resolve(oldabs);
    if (!ramfs_valid(idx)) return -1;
    split_path(newabs, parent, sizeof(parent), leaf, sizeof(leaf));
    int pidx = ramfs_resolve(parent);
    if (!ramfs_valid(pidx) || !ramfs_is_dir(pidx)) return -1;
    return ramfs_rename(idx, pidx, leaf) ? 0 : -1;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n < 0) return n;
    if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
    return (int)fwrite(buf, 1, (size_t)n, stream);
}

int fprintf(FILE *stream, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}

#define POPEN_BUF_SIZE 8192
static void popen_append(char *buf, int *len, const char *s) {
    while (*s && *len < POPEN_BUF_SIZE - 1) buf[(*len)++] = *s++;
}

static void run_builtin(const char *cmd, char *out, int *outlen) {
    if (strncmp(cmd, "ls", 2) == 0 && (cmd[2] == '\0' || cmd[2] == ' ')) {
        const char *arg = cmd[2] == ' ' ? cmd + 3 : NULL;
        char abs[512];
        to_absolute(arg && *arg ? arg : ".", abs, sizeof(abs));
        int idx = ramfs_resolve(abs);
        if (!ramfs_valid(idx) || !ramfs_is_dir(idx)) {
            popen_append(out, outlen, "ls: no such directory\n");
            return;
        }
        int cursor = 0, child;
        bool any = false;
        while ((child = ramfs_next_child(idx, &cursor)) >= 0) {
            popen_append(out, outlen, ramfs_name(child));
            popen_append(out, outlen, ramfs_is_dir(child) ? "/\n" : "\n");
            any = true;
        }
        if (!any) popen_append(out, outlen, "(empty)\n");
    } else if (strcmp(cmd, "pwd") == 0) {
        popen_append(out, outlen, g_cwd);
        popen_append(out, outlen, "\n");
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        char abs[512];
        to_absolute(cmd + 4, abs, sizeof(abs));
        int idx = ramfs_resolve(abs);
        if (!ramfs_valid(idx) || ramfs_is_dir(idx)) {
            popen_append(out, outlen, "cat: no such file\n");
        } else {
            char chunk[512];
            int off = 0, got;
            while ((got = ramfs_read(idx, off, chunk, sizeof(chunk) - 1)) > 0 && *outlen < POPEN_BUF_SIZE - 1) {
                chunk[got] = '\0';
                popen_append(out, outlen, chunk);
                off += got;
            }
        }
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        popen_append(out, outlen, cmd + 5);
        popen_append(out, outlen, "\n");
    } else if (strncmp(cmd, "mkdir ", 6) == 0) {
        if (mkdir(cmd + 6, 0755) == 0) popen_append(out, outlen, "");
        else popen_append(out, outlen, "mkdir: failed\n");
    } else if (strncmp(cmd, "rm ", 3) == 0) {
        if (remove(cmd + 3) == 0) popen_append(out, outlen, "");
        else popen_append(out, outlen, "rm: failed\n");
    } else if (strncmp(cmd, "touch ", 6) == 0) {
        FILE *f = fopen(cmd + 6, "a");
        if (f) { fclose(f); } else popen_append(out, outlen, "touch: failed\n");
    } else {
        popen_append(out, outlen, "command not found (bare-metal build has no process exec — try: ls, pwd, cat, echo, mkdir, rm, touch, download)\n");
    }
}

FILE *popen(const char *command, const char *type) {
    (void)type;
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "%s", command);
    size_t l = strlen(cmd);
    if (l > 5 && strcmp(cmd + l - 5, " 2>&1") == 0) cmd[l - 5] = '\0';

    char *buf = (char *)malloc(POPEN_BUF_SIZE);
    if (!buf) return NULL;
    int len = 0;
    run_builtin(cmd, buf, &len);

    MyFile *f = (MyFile *)malloc(sizeof(MyFile));
    if (!f) { free(buf); return NULL; }
    f->is_popen = true;
    f->pop_buf = buf;
    f->pop_len = len;
    f->pop_pos = 0;
    f->node_idx = -1;
    return (FILE *)f;
}

int pclose(FILE *stream) {
    MyFile *f = (MyFile *)stream;
    if (f->pop_buf) free(f->pop_buf);
    free(f);
    return 0;
}

int system(const char *command) {
    if (strncmp(command, "rm -rf \"", 8) == 0) {
        char path[512];
        const char *start = command + 8;
        const char *end = strchr(start, '"');
        if (!end) return -1;
        size_t len = (size_t)(end - start);
        if (len >= sizeof(path)) len = sizeof(path) - 1;
        memcpy(path, start, len);
        path[len] = '\0';
        int idx = ramfs_resolve(path);
        if (!ramfs_valid(idx)) return -1;
        return ramfs_delete(idx) ? 0 : -1;
    }
    return -1;
}
