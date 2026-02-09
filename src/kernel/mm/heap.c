/**
 * Minimal kernel heap: bump allocator (no free).
 * For production, replace with a free-list or similar.
 */

#include <kernel/mm.h>
#include <kernel/types.h>

#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

static uint8_t *heap_start;
static uint8_t *heap_cur;
static uint8_t *heap_end;

void heap_init(void *region, size_t size)
{
    heap_start = (uint8_t *)region;
    heap_cur   = heap_start;
    heap_end   = heap_start + size;
}

void *kmalloc(size_t size)
{
    size = ALIGN_UP(size, 8);
    if (heap_cur + size > heap_end)
        return NULL;
    void *p = heap_cur;
    heap_cur += size;
    return p;
}

void kfree(void *ptr)
{
    (void)ptr;
    /* Bump allocator: no free */
}
