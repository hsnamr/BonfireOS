# BonfireOS

A minimal, modular x86_64 operating system from scratch: bootloader → kernel → drivers → filesystem → shell.

## Features

- **Boot**: GRUB (Multiboot 1) loads the kernel; 32-bit boot stub switches to long mode and jumps to 64-bit kernel.
- **Kernel**: C + assembly, with IDT, PIC, and basic interrupt handling.
- **Drivers**: VGA text mode (80×25), PS/2 keyboard.
- **FS**: In-memory minimal filesystem (mkdir, create, read, write, list, chdir). No persistence.
- **Shell**: Commands: `help`, `clear`, `echo`, `ls`, `cd`, `mkdir`, `cat`, `edit`, `alias`.

## Quick start

1. **Install toolchain** (see [docs/BUILD.md](docs/BUILD.md)):
   - `nasm`
   - `x86_64-elf-gcc` and `x86_64-elf-ld` (cross-compiler)
   - For ISO: `grub-mkrescue`
   - For run: `qemu-system-x86_64`

2. **Build and run**
   ```bash
   make          # build kernel
   make run      # run in QEMU (direct kernel)
   make iso      # build bootable ISO
   make run-iso  # run ISO in QEMU
   ```

3. **In the shell**
   - `help` — list commands  
   - `ls` / `ls /path` — list directory  
   - `cd /path` — change directory  
   - `mkdir /path` — create directory  
   - `cat file` — print file  
   - `edit file` — create or overwrite file (single line)  
   - `alias ll ls` — alias `ll` to `ls`  
   - `echo hello` — print text  
   - `clear` — clear screen  

## Layout

```
BonfireOS/
├── Makefile           # Build: kernel, ISO, QEMU targets
├── linker.ld          # Kernel link layout (sections, stack)
├── scripts/grub.cfg   # GRUB menu for ISO
├── include/kernel/    # Headers (vga, port, idt, irq, keyboard, fs, shell, alias)
├── src/
│   ├── boot/boot.asm      # Multiboot header, 32-bit init, long mode switch, 64-bit entry
│   ├── kernel/kernel.c    # kernel_main: multiboot parse, init, shell
│   ├── kernel/arch/       # idt.c, idt_asm.asm, irq.c
│   ├── kernel/drivers/   # vga.c, keyboard.c
│   ├── kernel/fs/fs.c     # In-memory FS
│   └── kernel/shell/      # shell.c, alias.c
└── docs/
    ├── BUILD.md
    └── ARCHITECTURE.md
```

## Milestones

1. **Bootloader** — GRUB + multiboot; assembly stub and long mode.
2. **Kernel init** — GDT, IDT, PIC, VGA, keyboard.
3. **Memory** — (Planned) paging, physical allocator, heap.
4. **Drivers** — VGA, keyboard (done); disk I/O (planned).
5. **Filesystem** — In-memory FS (done); FAT or persistent FS (planned).
6. **Shell** — Commands and alias (done).

## License

MIT or similar; use and extend as you like.
