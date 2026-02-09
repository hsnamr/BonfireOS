; IDT handler stubs: call C handler with vector number, then EOI for IRQs
; IRQ 0 (vector 32): timer -> scheduler_tick then context switch

extern idt_irq_handler
extern idt_exception_handler
extern scheduler_tick

%macro IRQ 1
global irq%1
irq%1:
    push qword %1
    jmp irq_common
%endmacro

; Timer IRQ: save state, call scheduler_tick(rsp), switch to returned rsp
global timer_irq
timer_irq:
    push qword 32
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    call scheduler_tick
    mov rsp, rax
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 8
    iretq

%macro EXC 1
global exc%1
exc%1:
    push qword %1
    jmp exc_common
%endmacro

irq_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, [rsp + 15*8]   ; vector number
    call idt_irq_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 8
    iretq

exc_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, [rsp + 15*8]
    call idt_exception_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 8
    iretq

; CPU exceptions 0-31
EXC 0
EXC 1
EXC 2
EXC 3
EXC 4
EXC 5
EXC 6
EXC 7
EXC 8
EXC 9
EXC 10
EXC 11
EXC 12
EXC 13
EXC 14
EXC 15
EXC 16
EXC 17
EXC 18
EXC 19
EXC 20
EXC 21
EXC 22
EXC 23
EXC 24
EXC 25
EXC 26
EXC 27
EXC 28
EXC 29
EXC 30
EXC 31

; IRQs 0-15 (PIC) — vector 32 = timer_irq, rest = irq33..irq47
IRQ 33
IRQ 34
IRQ 35
IRQ 36
IRQ 37
IRQ 38
IRQ 39
IRQ 40
IRQ 41
IRQ 42
IRQ 43
IRQ 44
IRQ 45
IRQ 46
IRQ 47
