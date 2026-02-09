/* BonfireOS Kernel Linker Script (x86_64)
 * Output: flat binary layout expected by multiboot2 bootloader.
 * Sections: .multiboot (header), .text, .rodata, .data, .bss, .stack
 */

ENTRY(_start)

SECTIONS
{
    /* Load kernel at 1 MiB (common convention; avoids low memory) */
    . = 1M;

    .multiboot ALIGN(8) : {
        KEEP(*(.multiboot))
    }

    .text ALIGN(4K) : {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Kernel stack: 64 KiB below end of BSS */
    .stack ALIGN(4K) : {
        __stack_bottom = .;
        . += 64K;
        __stack_top = .;
    }

    __kernel_end = .;

    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.comment)
    }
}
