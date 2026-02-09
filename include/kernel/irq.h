#ifndef BONFIRE_IRQ_H
#define BONFIRE_IRQ_H

#include <kernel/types.h>

/* PIC ports */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20

/* Remap IRQs to IDT vectors 32-47 (avoid CPU exceptions 0-31) */
#define IRQ_BASE  32
#define IRQ0      (IRQ_BASE + 0)
#define IRQ1      (IRQ_BASE + 1)   /* keyboard */

void irq_init(void);
void irq_eoi(uint8_t irq);
void irq_mask_set(uint8_t irq);
void irq_mask_clear(uint8_t irq);

#endif /* BONFIRE_IRQ_H */
