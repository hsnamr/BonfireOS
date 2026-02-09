#ifndef BONFIRE_IDT_H
#define BONFIRE_IDT_H

#include <kernel/types.h>

#define IDT_ENTRIES 256

/* IDT gate types */
#define IDT_TYPE_INTR  0x0E   /* 64-bit interrupt gate */
#define IDT_TYPE_TRAP  0x0F   /* 64-bit trap gate */

void idt_init(void);
void idt_set_gate(uint8_t n, uint64_t handler, uint16_t selector, uint8_t type);

#endif /* BONFIRE_IDT_H */
