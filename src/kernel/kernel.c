/**
 * BonfireOS - Kernel main entry (64-bit)
 * Called from boot.asm after long mode is enabled.
 * Responsibilities: parse multiboot, init drivers, processes, start shell.
 */

#include <kernel/types.h>
#include <kernel/multiboot.h>
#include <kernel/vga.h>
#include <kernel/port.h>
#include <kernel/idt.h>
#include <kernel/irq.h>
#include <kernel/keyboard.h>
#include <kernel/shell.h>
#include <kernel/process.h>
#include <kernel/timer.h>
#include <kernel/mm.h>
#include <kernel/fat.h>

#define MULTIBOOT_FLAG_MEM   (1 << 0)
#define HEAP_SIZE            (256 * 1024)

static uint8_t heap_region[HEAP_SIZE];

extern void shell_run(void);

void kernel_main(uint32_t magic, uint32_t multiboot_info_phys)
{
    (void)magic;
    (void)multiboot_info_phys;

    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("BonfireOS - minimal x86_64 kernel\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("Boot successful. Kernel is running in long mode.\n");

    if (magic != MULTIBOOT_MAGIC) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_puts("Invalid multiboot magic.\n");
    } else {
        struct multiboot_info *mb = (struct multiboot_info *)(uint64_t)multiboot_info_phys;
        if (mb->flags & MULTIBOOT_FLAG_MEM) {
            vga_puts("Memory: lower=");
            vga_putdec(mb->mem_lower);
            vga_puts(" KB, upper=");
            vga_putdec(mb->mem_upper);
            vga_puts(" KB\n");
        }
    }

    heap_init(heap_region, HEAP_SIZE);
    shell_init();
    irq_init();
    idt_init();
    process_init();
    process_create(shell_run);
    timer_init(100);
    if (fat_mount() == 0)
        vga_puts("FAT filesystem mounted.\n");
    __asm__ volatile ("sti");

    vga_puts("\n> ");
    scheduler_first_run();
}
