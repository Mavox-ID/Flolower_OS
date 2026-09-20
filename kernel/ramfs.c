#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern void *malloc(size_t size);
extern void *memcpy(void *dst, const void *src, size_t n);
extern void *memset(void *s, int c, size_t n);
extern int strcmp(const char *a, const char *b);
extern size_t strlen(const char *s);
extern char *strncpy(char *dst, const char *src, size_t n);

#define RAMFS_MAX_NODES     512
#define RAMFS_NAME_LEN      64
#define RAMFS_FILE_INITIAL  1024
#define RAMFS_FILE_MAX      (256 * 1024)

typedef struct {
    bool used;
    bool is_dir;
    char name[RAMFS_NAME_LEN];
    int  parent;
    char *data;
    int   size;
    int   capacity;
} RamNode;

static RamNode nodes[RAMFS_MAX_NODES];
static bool initialized = false;

static void ramfs_ensure_init(void) {
    if (initialized) return;
    memset(nodes, 0, sizeof(nodes));
    nodes[0].used = true;
    nodes[0].is_dir = true;
    nodes[0].name[0] = '/';
    nodes[0].name[1] = '\0';
    nodes[0].parent = -1;
    initialized = true;
}

static int alloc_node(void) {
    for (int i = 1; i < RAMFS_MAX_NODES; i++)
        if (!nodes[i].used) return i;
    return -1;
}

static int find_child(int parent_idx, const char *name) {
    for (int i = 0; i < RAMFS_MAX_NODES; i++)
        if (nodes[i].used && nodes[i].parent == parent_idx && strcmp(nodes[i].name, name) == 0)
            return i;
    return -1;
}

int ramfs_resolve(const char *path) {
    ramfs_ensure_init();
    if (!path || path[0] != '/') return -1;
    int cur = 0;
    const char *p = path + 1;
    if (*p == '\0') return cur;
    char comp[RAMFS_NAME_LEN];
    while (*p) {
        int i = 0;
        while (*p && *p != '/' && i < RAMFS_NAME_LEN - 1) comp[i++] = *p++;
        comp[i] = '\0';
        if (*p == '/') p++;
        if (i == 0) continue;
        int next = find_child(cur, comp);
        if (next < 0) return -1;
        cur = next;
    }
    return cur;
}

bool ramfs_is_dir(int idx)          { return idx >= 0 && nodes[idx].used && nodes[idx].is_dir; }
bool ramfs_valid(int idx)           { return idx >= 0 && nodes[idx].used; }
const char *ramfs_name(int idx)     { return nodes[idx].name; }
int  ramfs_parent(int idx)          { return nodes[idx].parent; }

int ramfs_create(int parent_idx, const char *name, bool is_dir) {
    ramfs_ensure_init();
    if (parent_idx < 0 || !nodes[parent_idx].used || !nodes[parent_idx].is_dir) return -1;
    int existing = find_child(parent_idx, name);
    if (existing >= 0) return existing;

    int idx = alloc_node();
    if (idx < 0) return -1;
    RamNode *n = &nodes[idx];
    n->used = true;
    n->is_dir = is_dir;
    strncpy(n->name, name, RAMFS_NAME_LEN - 1);
    n->name[RAMFS_NAME_LEN - 1] = '\0';
    n->parent = parent_idx;
    n->data = NULL;
    n->size = 0;
    n->capacity = 0;
    return idx;
}

int ramfs_next_child(int dir_idx, int *cursor) {
    for (int i = *cursor; i < RAMFS_MAX_NODES; i++) {
        if (nodes[i].used && nodes[i].parent == dir_idx) {
            *cursor = i + 1;
            return i;
        }
    }
    *cursor = RAMFS_MAX_NODES;
    return -1;
}

static void free_subtree(int idx) {
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (nodes[i].used && nodes[i].parent == idx) free_subtree(i);
    }
    nodes[idx].used = false;
    nodes[idx].data = NULL;
}

bool ramfs_delete(int idx) {
    if (idx <= 0 || !nodes[idx].used) return false;
    free_subtree(idx);
    return true;
}

bool ramfs_rename(int idx, int new_parent_idx, const char *new_name) {
    if (idx <= 0 || !nodes[idx].used) return false;
    if (new_parent_idx < 0 || !nodes[new_parent_idx].used || !nodes[new_parent_idx].is_dir) return false;
    strncpy(nodes[idx].name, new_name, RAMFS_NAME_LEN - 1);
    nodes[idx].name[RAMFS_NAME_LEN - 1] = '\0';
    nodes[idx].parent = new_parent_idx;
    return true;
}

int ramfs_read(int idx, int offset, void *out, int max_len) {
    if (idx < 0 || !nodes[idx].used || nodes[idx].is_dir) return -1;
    RamNode *n = &nodes[idx];
    if (offset >= n->size) return 0;
    int avail = n->size - offset;
    int n_copy = (avail < max_len) ? avail : max_len;
    memcpy(out, n->data + offset, (size_t)n_copy);
    return n_copy;
}

int ramfs_write(int idx, int offset, const void *in, int len) {
    if (idx < 0 || !nodes[idx].used || nodes[idx].is_dir) return -1;
    RamNode *n = &nodes[idx];
    int need = offset + len;
    if (need > RAMFS_FILE_MAX) { len = RAMFS_FILE_MAX - offset; need = RAMFS_FILE_MAX; if (len <= 0) return 0; }

    if (need > n->capacity) {
        int newcap = n->capacity > 0 ? n->capacity : RAMFS_FILE_INITIAL;
        while (newcap < need) newcap *= 2;
        if (newcap > RAMFS_FILE_MAX) newcap = RAMFS_FILE_MAX;
        char *nd = (char *)malloc((size_t)newcap);
        if (!nd) return 0;
        if (n->data && n->size > 0) memcpy(nd, n->data, (size_t)n->size);
        n->data = nd;
        n->capacity = newcap;
    }
    memcpy(n->data + offset, in, (size_t)len);
    if (need > n->size) n->size = need;
    return len;
}

int ramfs_truncate(int idx) {
    if (idx < 0 || !nodes[idx].used || nodes[idx].is_dir) return -1;
    nodes[idx].size = 0;
    return 0;
}

int ramfs_size(int idx) {
    if (idx < 0 || !nodes[idx].used) return -1;
    return nodes[idx].size;
}
