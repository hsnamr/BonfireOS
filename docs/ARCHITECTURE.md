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

## Process and scheduling

- **PCB**: pid, state, saved_rsp, kernel_stack; processes in a circular run list.
- **Context switch**: Timer IRQ (vector 32) pushes state, calls `scheduler_tick(current_rsp)`; scheduler saves rsp to current PCB, picks next, returns next PCB's saved_rsp; assembly loads new rsp and iretq.
- **First run**: `scheduler_first_run()` sets current to run list head and `context_switch_to(rip=shell_run)` so the shell runs as the main process; idle process runs when preempted.
- **Heap**: Bump allocator for process kernel stacks; init from static region in kernel_main.

## Disk and FAT

- **ATA PIO**: Primary master, LBA28; `ata_read_sectors` / `ata_write_sectors`.
- **FAT**: `fat_mount()` reads BPB from LBA 0; FAT12/16 detected by cluster count. `fat_find_root(name_8_3)` finds file in root; `fat_read_file(cluster, size, buf)` follows FAT chain. Shell command `fatcat FILE.TXT` reads from disk.

## POSIX compatibility

- **File descriptors**: 0=stdin (keyboard), 1/2=stdout/stderr (VGA); 3+ from `open(path)` (in-memory FS).
- **API**: open, read, write, close, lseek, getcwd, chdir, mkdir, stat; errno set on error.
- Implementations in `posix/posix.c` use fs_* and VGA/keyboard; no syscall gate yet (direct kernel calls).

## Extensions (planned)

- Physical frame allocator (e.g. bitmap from multiboot memory map).
- Free-list heap for kernel allocations.
- FAT write support; mount FAT at a path.
- Syscall gate (e.g. int 0x80) for userland.
