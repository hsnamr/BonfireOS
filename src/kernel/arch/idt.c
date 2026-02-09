/**
 * IDT (Interrupt Descriptor Table) setup for x86_64.
 * Loads the table and sets gate addresses (handlers in idt_asm.asm).
 */

#include <kernel/idt.h>
#include <kernel/irq.h>
#include <kernel/keyboard.h>
#include <kernel/port.h>

/* External assembly handlers - exceptions 0-31 */
extern void exc0(void),  exc1(void),  exc2(void),  exc3(void),  exc4(void),  exc5(void),  exc6(void),  exc7(void);
extern void exc8(void),  exc9(void),  exc10(void), exc11(void), exc12(void), exc13(void), exc14(void), exc15(void);
extern void exc16(void), exc17(void), exc18(void), exc19(void), exc20(void), exc21(void), exc22(void), exc23(void);
extern void exc24(void), exc25(void), exc26(void), exc27(void), exc28(void), exc29(void), exc30(void), exc31(void);
/* IRQs 32-47 */
extern void irq32(void);
extern void irq33(void);
extern void irq34(void);
extern void irq35(void);
extern void irq36(void);
extern void irq37(void);
extern void irq38(void);
extern void irq39(void);
extern void irq40(void);
extern void irq41(void);
extern void irq42(void);
extern void irq43(void);
extern void irq44(void);
extern void irq45(void);
extern void irq46(void);
extern void irq47(void);

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

static void set_gate(uint8_t n, uint64_t handler, uint16_t sel, uint8_t type)
{
    idt[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[n].selector    = sel;
    idt[n].ist         = 0;
    idt[n].type_attr   = type | 0x60; /* DPL=0, present */
    idt[n].zero        = 0;
}

void idt_set_gate(uint8_t n, uint64_t handler, uint16_t selector, uint8_t type)
{
    set_gate(n, handler, selector, type);
}

void idt_init(void)
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)idt;

    static const void *exc_handlers[] = {
        (void *)exc0, (void *)exc1, (void *)exc2, (void *)exc3, (void *)exc4, (void *)exc5, (void *)exc6, (void *)exc7,
        (void *)exc8, (void *)exc9, (void *)exc10, (void *)exc11, (void *)exc12, (void *)exc13, (void *)exc14, (void *)exc15,
        (void *)exc16, (void *)exc17, (void *)exc18, (void *)exc19, (void *)exc20, (void *)exc21, (void *)exc22, (void *)exc23,
        (void *)exc24, (void *)exc25, (void *)exc26, (void *)exc27, (void *)exc28, (void *)exc29, (void *)exc30, (void *)exc31,
    };
    for (int i = 0; i < 32; i++)
        set_gate(i, (uint64_t)exc_handlers[i], 0x08, IDT_TYPE_INTR);

    /* IRQ 0-15 -> vectors 32-47 */
    uint64_t *irq_handlers[] = {
        (uint64_t *)irq32, (uint64_t *)irq33, (uint64_t *)irq34, (uint64_t *)irq35,
        (uint64_t *)irq36, (uint64_t *)irq37, (uint64_t *)irq38, (uint64_t *)irq39,
        (uint64_t *)irq40, (uint64_t *)irq41, (uint64_t *)irq42, (uint64_t *)irq43,
        (uint64_t *)irq44, (uint64_t *)irq45, (uint64_t *)irq46, (uint64_t *)irq47,
    };
    for (int i = 0; i < 16; i++)
        set_gate(IRQ_BASE + i, (uint64_t)irq_handlers[i], 0x08, IDT_TYPE_INTR);

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void idt_irq_handler(uint64_t vector)
{
    if (vector >= IRQ_BASE && vector < IRQ_BASE + 16)
        irq_eoi((uint8_t)(vector - IRQ_BASE));
    if (vector == IRQ_BASE + 1)
        keyboard_irq_handler();
}

void idt_exception_handler(uint64_t vector)
{
    (void)vector;
    /* TODO: print exception and halt or continue */
    for (;;) __asm__ volatile ("hlt");
}
