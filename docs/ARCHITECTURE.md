# BonfireOS Architecture

## Boot flow

1. **GRUB** loads the kernel at 1 MiB (multiboot convention), passes multiboot magic (0x2BADB002) and info structure address in `EAX`/`EBX`, and jumps to `_start` in 32-bit mode.

2. **boot.asm** (32-bit):
   - Saves multiboot magic and info.
   - Clears BSS.
   - Sets up identity page tables for the first 2 MiB (PML4 → PDPT → PD with one 2 MiB page).
   - Enables PAE, EFER.LME, then paging (CR0.PG).
   - Loads a minimal GDT (64-bit code and data).
   - Long jump to 64-bit code segment (`long_mode_entry`).

3. **boot.asm** (64-bit):
   - Sets segment registers and stack.
   - Calls `kernel_main(magic, multiboot_info)`.

4. **kernel_main** (C):
   - Prints boot message and memory info from multiboot.
   - Inits PIC (remap IRQs to 32–47), IDT (exceptions + IRQs), then `sti`.
   - Inits filesystem and shell, prints `> `, and enters `shell_run()` (read line → expand aliases → run command).

## Memory map (current)

- **0x100000 (1 MiB)** — Kernel load address (multiboot).
- **0xB8000** — VGA text framebuffer (80×25, 16 colors).
- **BSS / stack** — After kernel sections (linker script); stack 64 KiB at end.

Paging: identity map only for the first 2 MiB; no high-half mapping yet.

## Interrupts

- **IDT**: 256 entries; 0–31 = CPU exceptions, 32–47 = PIC IRQs.
- **PIC**: Master 0x20/0x21, slave 0xA0/0xA1; IRQs remapped to 32–47; IRQ0 (timer) and IRQ1 (keyboard) unmasked.
- **Handlers**: Assembly stubs push vector, save registers, call C `idt_irq_handler` or `idt_exception_handler`; IRQ handler sends EOI and calls `keyboard_irq_handler()` for IRQ1.

## Drivers

- **VGA**: Direct write to 0xB8000; cursor via row/column; scroll on newline at bottom.
- **Keyboard**: PS/2 port 0x60; scancode set 1 → ASCII in a ring buffer; `keyboard_getchar()` is non-blocking.

## Filesystem

- In-memory only; single root; fixed max files and dirs.
- Each file has a static 4 KiB buffer (no dynamic allocator yet).
- Paths: absolute or relative to CWD; `..` and `.` normalized.
- Operations: mkdir, create, read, write, list, chdir, get_cwd, exists.

## Shell

- Line buffer; on Enter, expand first word via alias table, then dispatch by command name.
- Commands implemented in `shell.c`; FS and alias in separate modules.

## Extensions (planned)

- Physical frame allocator (e.g. bitmap from multiboot memory map).
- Heap (e.g. bump or free-list) for kernel allocations.
- Disk driver (ATA PIO or AHCI) and a simple persistent FS (e.g. FAT12/16 or custom).
- Process/scheduler (optional) and system calls.
