; Context switch: load new stack pointer and return via iretq (as from interrupt).
; void context_switch_to(uint64_t new_rsp);
; The stack at new_rsp must have: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8..r15, vector, iret frame.

global context_switch_to
context_switch_to:
    mov rsp, rdi
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
    add rsp, 8   ; skip vector
    iretq
