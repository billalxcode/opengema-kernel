// Callee-saved context switch and new thread entry.

    .section .text
    .global SwitchContext
SwitchContext:
    sub  sp, sp, #96
    stp  x19, x20, [sp, #0]
    stp  x21, x22, [sp, #16]
    stp  x23, x24, [sp, #32]
    stp  x25, x26, [sp, #48]
    stp  x27, x28, [sp, #64]
    stp  x29, x30, [sp, #80]
    mov  x2, sp
    str  x2, [x0]
    mov  sp, x1
    ldp  x19, x20, [sp, #0]
    ldp  x21, x22, [sp, #16]
    ldp  x23, x24, [sp, #32]
    ldp  x25, x26, [sp, #48]
    ldp  x27, x28, [sp, #64]
    ldp  x29, x30, [sp, #80]
    add  sp, sp, #96
    ret

    .global ThreadStartTrampoline
ThreadStartTrampoline:
    msr  daifclr, #2
    mov  x0, x20
    blr  x19
    bl   SchedulerThreadExit
1:  b    1b

// void EnterUserMode(uint64_t entry, uint64_t stack_pointer)
    .global EnterUserMode
EnterUserMode:
    msr  elr_el1, x0
    msr  sp_el0, x1
    msr  spsr_el1, xzr
    eret
