# BonfireOS - Minimal x86_64 OS Build System
# Uses cross-compiler: x86_64-elf-gcc, x86_64-elf-ld, nasm
# Run: make, make run (QEMU), make iso (bootable image)

ARCH     := x86_64
CC       := x86_64-elf-gcc
LD       := x86_64-elf-ld
AS       := nasm
OBJCOPY  := x86_64-elf-objcopy
GRUB_MKRESCUE := grub-mkrescue

# Optional GUI (1980s-style). Set ENABLE_GUI=0 to build without GUI.
ENABLE_GUI ?= 1

# Flags
CFLAGS   := -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
            -m64 -march=x86-64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
            -Wall -Wextra -O2 -g -I include -DENABLE_GUI=$(ENABLE_GUI)
ASFLAGS  := -f elf64
LDFLAGS  := -nostdlib -static -z max-page-size=0x1000 -T linker.ld

# Directories
BUILD    := build
OBJ      := $(BUILD)/obj
BOOT     := $(BUILD)/boot
ISO      := $(BUILD)/iso
ISO_BOOT := $(ISO)/boot
GRUB_CFG := scripts/grub.cfg

# Kernel objects (C and ASM) — mirror src/ under build/obj/
# When ENABLE_GUI=0, exclude GUI sources so the kernel builds without GUI.
C_SRCS_ALL := $(shell find src -name '*.c')
GUI_SRCS   := $(wildcard src/kernel/gui/*.c)
ifeq ($(ENABLE_GUI),0)
C_SRCS     := $(filter-out $(GUI_SRCS),$(C_SRCS_ALL))
else
C_SRCS     := $(C_SRCS_ALL)
endif
ASM_SRCS   := $(shell find src -name '*.asm' -o -name '*.s')
C_OBJS   := $(patsubst src/%.c,$(OBJ)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst src/%.asm,$(OBJ)/%.o,$(filter %.asm,$(ASM_SRCS)))
ASM_OBJS += $(patsubst src/%.s,$(OBJ)/%.o,$(filter %.s,$(ASM_SRCS)))
OBJS     := $(C_OBJS) $(ASM_OBJS)

# Targets
KERNEL_BIN := $(BUILD)/kernel.bin
ISO_IMG    := $(BUILD)/bonfireos.iso

.PHONY: all clean run iso dirs no-gui

all: dirs $(KERNEL_BIN)

# Build without the optional GUI (CLI-only kernel).
no-gui:
	$(MAKE) all ENABLE_GUI=0

dirs:
	@mkdir -p $(BOOT) $(OBJ) $(shell dirname $(C_OBJS)) $(shell dirname $(ASM_OBJS))

# Compile C sources
$(OBJ)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM sources
$(OBJ)/%.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ)/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Link kernel
$(KERNEL_BIN): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# Build bootable ISO (for QEMU and VirtualBox)
iso: all
	@mkdir -p $(ISO_BOOT)/grub
	cp $(KERNEL_BIN) $(ISO_BOOT)/
	cp $(GRUB_CFG) $(ISO_BOOT)/grub/
	$(GRUB_MKRESCUE) -o $(ISO_IMG) $(ISO)
	@echo "ISO image: $(ISO_IMG)"

# Run in QEMU (kernel only, no ISO - faster for dev)
run: all
	qemu-system-x86_64 -kernel $(KERNEL_BIN) -serial stdio -no-reboot -no-shutdown

# Run from ISO (closer to real hardware / VirtualBox)
run-iso: iso
	qemu-system-x86_64 -cdrom $(ISO_IMG) -serial stdio -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD)

# Check for cross-compiler
check-toolchain:
	@which $(CC) >/dev/null 2>&1 || (echo "Install x86_64-elf cross compiler. See docs/BUILD.md"; exit 1)
	@which $(AS) >/dev/null 2>&1 || (echo "Install nasm"; exit 1)
