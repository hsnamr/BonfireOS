#ifndef BONFIRE_PROCESS_H
#define BONFIRE_PROCESS_H

#include <kernel/types.h>

#define PROCESS_STACK_SIZE  (64 * 1024)   /* 64 KiB per process */
#define MAX_PROCESSES       8

enum process_state {
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_BLOCKED,
};

struct process {
    uint64_t pid;
    enum process_state state;
    uint64_t saved_rsp;           /* kernel stack pointer when not running */
    uint8_t *kernel_stack;        /* base of allocated stack */
    struct process *next;         /* round-robin list */
};

void process_init(void);
struct process *process_current(void);
void process_create(void (*entry)(void));
/* Called from timer IRQ; returns new rsp to switch to (may be same). */
uint64_t scheduler_tick(uint64_t current_rsp);
/* Start running the first process (call once after creating processes). */
void scheduler_first_run(void);

extern void context_switch_to(uint64_t new_rsp);

#endif /* BONFIRE_PROCESS_H */
