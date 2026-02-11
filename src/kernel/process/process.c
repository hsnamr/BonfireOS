/**
 * Process table and round-robin scheduler.
 * Each process has a kernel stack; context is saved on stack during interrupt.
 */

#include <kernel/process.h>
#include <kernel/mm.h>
#include <kernel/irq.h>
#include <kernel/timer.h>
#include <kernel/types.h>

#define STACK_ALIGN 16

static struct process processes[MAX_PROCESSES];
static size_t process_count;
static struct process *current_process;
static struct process *run_list;   /* circular list of runnable processes */
static uint64_t next_pid;

static void idle_loop(void)
{
    for (;;) __asm__ volatile ("hlt");
}

static struct process *alloc_process(void)
{
    if (process_count >= MAX_PROCESSES) return NULL;
    return &processes[process_count++];
}

void process_init(void)
{
    process_count = 0;
    current_process = NULL;
    run_list = NULL;
    next_pid = 1;
    process_create(idle_loop);
}

struct process *process_current(void)
{
    return current_process;
}

static void process_setup_stack(struct process *p, void (*entry)(void))
{
    uint8_t *stack_top = p->kernel_stack + PROCESS_STACK_SIZE;
    stack_top = (uint8_t *)((uintptr_t)stack_top & ~(STACK_ALIGN - 1));
    /* Layout (low to high): rax..r15, vector, rip, cs, rflags, rsp, ss. iretq pops rip,cs,rflags,rsp,ss. */
    stack_top -= 8 * 15;  /* space for rax..r15 */
    uint8_t *saved = stack_top;
    stack_top -= 8;
    *(uint64_t *)stack_top = 32;                /* vector */
    stack_top -= 8;
    *(uint64_t *)stack_top = (uint64_t)entry;   /* rip */
    stack_top -= 8;
    *(uint64_t *)stack_top = 0x08;              /* cs */
    stack_top -= 8;
    *(uint64_t *)stack_top = 0x202;             /* rflags */
    stack_top -= 8;
    *(uint64_t *)stack_top = 0;                 /* rsp (dummy) */
    stack_top -= 8;
    *(uint64_t *)stack_top = 0x10;              /* ss */
    for (int i = 0; i < 15; i++) *(uint64_t *)(saved + i * 8) = 0;
    p->saved_rsp = (uint64_t)saved;
}

void process_create(void (*entry)(void))
{
    struct process *p = alloc_process();
    if (!p) return;
    p->kernel_stack = (uint8_t *)kmalloc(PROCESS_STACK_SIZE);
    if (!p->kernel_stack) return;
    p->pid = next_pid++;
    p->state = PROC_RUNNABLE;
    p->next = NULL;
    process_setup_stack(p, entry);
    if (!run_list) {
        run_list = p;
        p->next = p;
    } else {
        p->next = run_list->next;
        run_list->next = p;
        run_list = p;
    }
}

uint64_t scheduler_tick(uint64_t current_rsp)
{
    irq_eoi(0);  /* timer IRQ0 */
    timer_tick();
    if (!current_process) return current_rsp;
    current_process->saved_rsp = current_rsp;
    current_process->state = PROC_RUNNABLE;
    struct process *next = current_process->next;
    if (!next) return current_rsp;
    current_process = next;
    current_process->state = PROC_RUNNING;
    return current_process->saved_rsp;
}

void process_set_current(struct process *p)
{
    current_process = p;
    if (p) p->state = PROC_RUNNING;
}

struct process *process_get_run_list(void)
{
    return run_list;
}

void process_add_runnable(struct process *p)
{
    if (!run_list) {
        run_list = p;
        p->next = p;
    } else {
        p->next = run_list->next;
        run_list->next = p;
    }
}

void scheduler_first_run(void)
{
    if (!run_list) return;
    current_process = run_list;
    current_process->state = PROC_RUNNING;
    context_switch_to(current_process->saved_rsp);
}
