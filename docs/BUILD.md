# Building BonfireOS

## Toolchain (x86_64-elf)

The kernel is built with a **cross-compiler** so it does not link against the host libc or runtime.

### Option 1: Install via package manager

- **Arch Linux**: `sudo pacman -S arm-none-eabi-gcc` does not provide x86_64-elf. Use below or [osdev.org toolchain](https://wiki.osdev.org/GCC_Cross-Compiler).
- **Ubuntu/Debian**: No official `x86_64-elf-gcc` package. Build the cross-compiler (Option 2) or use a Docker image.

### Option 2: Build GCC cross-compiler (recommended)

Use the project script to download and build **binutils** and **GCC** for `x86_64-elf`. Install prefix defaults to `$HOME/.local` (no sudo needed for the toolchain itself).

```bash
# Install dependencies (Ubuntu/Debian) — run once, requires sudo
sudo apt install -y build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo nasm curl

# Build and install toolchain (~30–60 min). Installs to $HOME/.local by default.
chmod +x scripts/install-toolchain.sh
./scripts/install-toolchain.sh

# Add to PATH (e.g. in ~/.bashrc)
export PATH="$HOME/.local/bin:$PATH"
```

Optional: install to a different prefix, or skip the dependency step if already installed:

```bash
./scripts/install-toolchain.sh /usr/local   # needs sudo for install
./scripts/install-toolchain.sh --no-deps    # skip apt step
```

See [GCC Cross-Compiler (OSDev)](https://wiki.osdev.org/GCC_Cross-Compiler) for manual steps.

### Option 3: Use Docker (if available)

A Dockerfile can provide a prebuilt environment with `x86_64-elf-gcc` and `nasm`; run `make` inside the container.

## Build commands

```bash
make          # Build kernel binary (build/kernel.bin)
make iso      # Build bootable ISO (build/bonfireos.iso)
make run      # Run kernel in QEMU (direct multiboot)
make run-iso  # Run ISO in QEMU
make clean    # Remove build/
```

## Requirements

- **nasm** (Netwide Assembler)
- **x86_64-elf-gcc** and **x86_64-elf-ld**
- **grub-mkrescue** (for `make iso`): `sudo apt install grub-pc-bin` (or equivalent)
- **QEMU** (for `make run`): `sudo apt install qemu-system-x86`

## Verify toolchain

```bash
make check-toolchain
```

If this fails, install or build the x86_64-elf toolchain and ensure it is in your `PATH`.

---

## Running the OS

### QEMU (recommended for development)

- **Direct kernel** (fast, no ISO):
  ```bash
  make run
  ```
  QEMU boots the kernel via `-kernel build/kernel.bin`. Use `-serial stdio` so you can type in the terminal.

- **From ISO** (closer to real boot):
  ```bash
  make run-iso
  ```
  Boots `build/bonfireos.iso` as a CD. Same keyboard/serial behavior.

- **Debug with GDB**: Start QEMU with `-s -S`, then in another terminal:
  ```bash
  gdb build/kernel.bin
  (gdb) target remote :1234
  (gdb) continue
  ```

### VirtualBox

1. Build the ISO: `make iso`
2. Create a new VM: Type **Other**, Version **Other/Unknown (64-bit)**.
3. Memory: 64 MB or more.
4. Add a virtual optical drive and attach `build/bonfireos.iso`.
5. Start the VM. Select "BonfireOS" in the GRUB menu.
6. Keyboard input goes to the VM; no guest additions required for basic use.
