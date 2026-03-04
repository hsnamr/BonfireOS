/**
 * Red Alert dedicated heap (malloc/free/realloc).
 * Same algorithm as DOOM heap; separate region so Red Alert port does not
 * depend on doom_* and both games can be linked if desired.
 */

#include <kernel/types.h>

#define REDALERT_HEAP_SIZE  (4 * 1024 * 1024)  /* 4 MiB for Red Alert (MIX, maps, etc.) */
#define ALIGN               8
#define ALIGN_UP(x)         (((x) + (ALIGN - 1)) & ~(ALIGN - 1))

static uint8_t redalert_heap[REDALERT_HEAP_SIZE];
static bool redalert_heap_inited;

struct block {
    size_t size;
    struct block *next;
};

static struct block *free_list;

static void redalert_heap_init(void)
{
    if (redalert_heap_inited) return;
    redalert_heap_inited = true;
    free_list = (struct block *)redalert_heap;
    free_list->size = REDALERT_HEAP_SIZE - sizeof(struct block);
    free_list->next = NULL;
}

void *redalert_malloc_impl(size_t size)
{
    if (size == 0) return NULL;
    redalert_heap_init();
    size = ALIGN_UP(size);
    struct block **p = &free_list;
    while (*p) {
        if ((*p)->size >= size) {
            struct block *b = *p;
            if (b->size >= size + sizeof(struct block) + ALIGN) {
                struct block *rest = (struct block *)((uint8_t *)b + sizeof(struct block) + size);
                rest->size = b->size - size - sizeof(struct block);
                rest->next = b->next;
                *p = rest;
                b->size = size;
            } else {
                *p = b->next;
            }
            return (void *)(b + 1);
        }
        p = &(*p)->next;
    }
    return NULL;
}

void redalert_free_impl(void *ptr)
{
    if (!ptr) return;
    redalert_heap_init();
    struct block *b = (struct block *)ptr - 1;
    struct block **p = &free_list;
    while (*p && (uint8_t *)*p < (uint8_t *)b) p = &(*p)->next;
    b->next = *p;
    *p = b;
    if (b->next && (uint8_t *)b + sizeof(struct block) + b->size == (uint8_t *)b->next) {
        b->size += sizeof(struct block) + b->next->size;
        b->next = b->next->next;
    }
}

void *redalert_realloc_impl(void *ptr, size_t new_size)
{
    if (!ptr) return redalert_malloc_impl(new_size);
    if (new_size == 0) { redalert_free_impl(ptr); return NULL; }
    struct block *b = (struct block *)ptr - 1;
    if (b->size >= new_size) return ptr;
    void *n = redalert_malloc_impl(new_size);
    if (!n) return NULL;
    for (size_t i = 0; i < b->size; i++) ((uint8_t *)n)[i] = ((uint8_t *)ptr)[i];
    redalert_free_impl(ptr);
    return n;
}
