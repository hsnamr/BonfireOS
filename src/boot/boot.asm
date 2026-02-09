; BonfireOS Boot Stub (x86_64 via Long Mode)
; 1) Multiboot 1: GRUB loads kernel, jumps here in 32-bit.
; 2) We set up minimal page tables, GDT, then switch to long mode.
; 3) We jump to 64-bit code which calls kernel_main(magic, multiboot_info).

%define MB_MAGIC  0x1BADB002
%define MB_FLAGS  0x00000003
%define PAGE_PRESENT   (1 << 0)
%define PAGE_WRITE     (1 << 1)
%define PAGE_HUGE      (1 << 7)
%define CR4_PAE        (1 << 5)
%define EFER_MSR       0xC0000080
%define EFER_LME       (1 << 8)
%define CR0_PG         (1 << 31)
%define CR0_PE         (1 << 0)

section .multiboot
align 4
multiboot_header:
    dd MB_MAGIC
    dd MB_FLAGS
    dd -(MB_MAGIC + MB_FLAGS)

section .bss
align 4096
; Page tables: identity map first 2 MiB (covers kernel at 1 MiB)
pml4:    resb 4096
pdpt:    resb 4096
pd:      resb 4096
; GDT for long mode (code + data)
gdt_desc:
    resw 1
    resq 1

section .data
align 4
; Passed from 32-bit to 64-bit
multiboot_magic: dd 0
multiboot_info:  dd 0

; GDT for long mode: null, code 0x08, data 0x10
gdt:
    dq 0
    ; 64-bit code: DPL=0, code, 64-bit
    dq 0x00AF9A000000FFFF
    ; 64-bit data
    dq 0x00AF92000000FFFF

section .text
global _start
extern __stack_top
extern __bss_start
extern __bss_end
extern kernel_main

_start:
    [bits 32]
    cld
    mov esp, __stack_top

    ; Save multiboot info (EBX = info, EAX = magic)
    mov dword [multiboot_magic], eax
    mov dword [multiboot_info], ebx

    ; Clear BSS
    mov edi, __bss_start
.clear_bss:
    cmp edi, __bss_end
    jge .bss_done
    mov byte [edi], 0
    inc edi
    jmp .clear_bss
.bss_done:

    ; Identity-map first 2 MiB: PML4[0] -> PDPT -> PD -> 2 MiB page
    mov eax, pdpt
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov dword [pml4], eax

    mov eax, pd
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov dword [pdpt], eax

    mov eax, 0
    or eax, PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE
    mov dword [pd], eax

    ; Load CR3 (PML4)
    mov eax, pml4
    mov cr3, eax

    ; Enable PAE
    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    ; Set EFER.LME (long mode enable)
    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LME
    wrmsr

    ; Enable paging (and protected mode if not already)
    mov eax, cr0
    or eax, CR0_PG | CR0_PE
    mov cr0, eax

    ; Load GDT
    mov eax, gdt
    mov dword [gdt_desc + 2], eax
    mov word [gdt_desc], 23
    lgdt [gdt_desc]

    ; Long jump to 64-bit code segment (selector 0x08)
    jmp 0x08:long_mode_entry

; ---------------------------------------------------------------------------
; 64-bit entry
; ---------------------------------------------------------------------------
[bits 64]
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, __stack_top

    ; kernel_main(uint32_t magic, uint32_t multiboot_info_phys)
    mov edi, dword [multiboot_magic]
    mov esi, dword [multiboot_info]
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
