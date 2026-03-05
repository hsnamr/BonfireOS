# BonfireOS

A minimal, modular x86_64 operating system from scratch: bootloader → kernel → drivers → filesystem → shell.

## Features

- **Boot**: GRUB (Multiboot 1) loads the kernel; 32-bit boot stub switches to long mode and jumps to 64-bit kernel.
- **Kernel**: C + assembly, with IDT, PIC, and basic interrupt handling.
- **Processes & scheduling**: Round-robin scheduler, PIT timer (~100 Hz), context switch; idle process + shell process.
- **Drivers**: VGA text mode (80×25), PS/2 keyboard, PIT timer, ATA PIO (disk).
- **FS**: In-memory minimal filesystem (mkdir, create, read, write, list, chdir). FAT12/16 read from disk (mount at boot, `fatcat FILE.TXT`).
- **POSIX layer**: `open`/`read`/`write`/`close`, `lseek`, `getcwd`/`chdir`/`mkdir`, `stat`; fd 0/1/2 = stdin/stdout/stderr.
- **Shell**: Commands: `help`, `clear`, `echo`, `ls`, `cd`, `mkdir`, `cat`, `edit`, `alias`, `fatcat`, `DOOM`.
- **DOOM host API**: Video (mode 13h), input (keyboard scancodes + mouse), time, malloc/free, file I/O. Type `DOOM` to run a linked DOOM port; see [docs/DOOM_PORT.md](docs/DOOM_PORT.md).

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
   - `fatcat FILE.TXT` — read file from FAT root on disk (8.3 name)  
   - `echo hello` — print text  
   - `clear` — clear screen  

## Layout

```
BonfireOS/
├── Makefile           # Build: kernel, ISO, QEMU targets
├── linker.ld          # Kernel link layout (sections, stack)
├── scripts/grub.cfg   # GRUB menu for ISO
├── include/kernel/   # Headers (vga, port, idt, irq, keyboard, fs, fat, ata, process, timer, posix, shell, alias)
├── src/
│   ├── boot/boot.asm         # Multiboot, long mode switch, 64-bit entry
│   ├── kernel/kernel.c       # kernel_main: init, process, shell
│   ├── kernel/arch/          # idt.c, idt_asm.asm, context_switch.asm, irq.c
│   ├── kernel/drivers/      # vga.c, keyboard.c, timer.c, ata.c
│   ├── kernel/fs/            # fs.c (in-memory), fat.c (FAT12/16)
│   ├── kernel/mm/heap.c     # Bump allocator
│   ├── kernel/process/      # process.c (PCB, scheduler)
│   ├── kernel/posix/        # posix.c (open/read/write/close, etc.)
│   └── kernel/shell/        # shell.c, alias.c
└── docs/
    ├── BUILD.md
    └── ARCHITECTURE.md
```

## Milestones

1. **Bootloader** — GRUB + multiboot; assembly stub and long mode.
2. **Kernel init** — GDT, IDT, PIC, VGA, keyboard.
3. **Process/scheduling** — PCB, context switch, PIT timer, round-robin (idle + shell).
4. **Memory** — Heap (bump allocator) for process stacks.
5. **Drivers** — VGA, keyboard, timer, ATA PIO (disk).
6. **Filesystem** — In-memory FS; FAT12/16 read from disk.
7. **POSIX layer** — open/read/write/close, getcwd/chdir/mkdir, stat.
8. **Shell** — Commands and alias, fatcat.

## License
GNU Affero General Public License v3.0
