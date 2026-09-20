#include <stddef.h>
#include <stdint.h>

#define HEAP_SIZE (4 * 1024 * 1024)

static uint8_t heap_arena[HEAP_SIZE];
static size_t  heap_used = 0;

void *malloc(size_t size) {
    size_t aligned = (size + 15) & ~((size_t)15);
    if (heap_used + aligned > HEAP_SIZE) return NULL;
    void *p = &heap_arena[heap_used];
    heap_used += aligned;
    return p;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) {
        uint8_t *b = (uint8_t *)p;
        for (size_t i = 0; i < total; i++) b[i] = 0;
    }
    return p;
}

void free(void *ptr) {
    (void)ptr;
}
