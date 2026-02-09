#ifndef BONFIRE_MM_H
#define BONFIRE_MM_H

#include <kernel/types.h>

/* Simple bump allocator (no free). For full heap, add free-list later. */
void heap_init(void *region, size_t size);
void *kmalloc(size_t size);
void kfree(void *ptr);  /* no-op for bump allocator */

#endif /* BONFIRE_MM_H */
