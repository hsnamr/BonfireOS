/**
 * BonfireOS - Kernel main entry (64-bit)
 * Called from boot.asm after long mode is enabled.
 * Responsibilities: parse multiboot, init drivers, start shell.
 */

#include <kernel/types.h>
#include <kernel/multiboot.h>
#include <kernel/vga.h>
#include <kernel/port.h>
#include <kernel/idt.h>
#include <kernel/irq.h>
#include <kernel/keyboard.h>
#include <kernel/shell.h>

#define MULTIBOOT_FLAG_MEM   (1 << 0)
#define MULTIBOOT_FLAG_MMAP  (1 << 6)

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

    irq_init();
    idt_init();
    __asm__ volatile ("sti");

    shell_init();
    vga_puts("\n> ");
    shell_run();
}
