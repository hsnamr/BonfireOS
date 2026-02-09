#ifndef BONFIRE_MULTIBOOT_H
#define BONFIRE_MULTIBOOT_H

#include <kernel/types.h>

#define MULTIBOOT_MAGIC 0x2BADB002

/* Multiboot info structure (relevant fields only) */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;   /* KB below 1M */
    uint32_t mem_upper;   /* KB above 1M */
    uint32_t boot_device;
    uint32_t cmdline;     /* physical addr of command line */
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;   /* physical addr of memory map */
    /* ... more optional fields ... */
} __attribute__((packed));

struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

#define MULTIBOOT_MEMORY_AVAILABLE 1

#endif /* BONFIRE_MULTIBOOT_H */
