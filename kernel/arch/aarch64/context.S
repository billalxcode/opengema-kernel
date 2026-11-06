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

// void EnterUserMode(uint64_t entry, uint64_t stack_pointer,
//                    uint64_t argc, uint64_t argv, uint64_t envp)
    .global EnterUserMode
EnterUserMode:
    msr  elr_el1, x0
    msr  sp_el0, x1
    mov  x0, x2
    mov  x1, x3
    mov  x2, x4
    msr  spsr_el1, xzr
    eret

// void EnterForkedUserMode(const uint64_t* registers, uint64_t elr,
//                          uint64_t spsr, uint64_t stack_pointer)
// registers points to x[0..30].
    .global EnterForkedUserMode
EnterForkedUserMode:
    msr  elr_el1, x1
    msr  spsr_el1, x2
    msr  sp_el0, x3
    mov  x9, x0
    ldp  x0, x1, [x9, #0]
    ldp  x2, x3, [x9, #16]
    ldp  x4, x5, [x9, #32]
    ldp  x6, x7, [x9, #48]
    ldr  x8, [x9, #64]
    ldp  x10, x11, [x9, #80]
    ldp  x12, x13, [x9, #96]
    ldp  x14, x15, [x9, #112]
    ldp  x16, x17, [x9, #128]
    ldp  x18, x19, [x9, #144]
    ldp  x20, x21, [x9, #160]
    ldp  x22, x23, [x9, #176]
    ldp  x24, x25, [x9, #192]
    ldp  x26, x27, [x9, #208]
    ldp  x28, x29, [x9, #224]
    ldr  x30, [x9, #240]
    ldr  x9, [x9, #72]
    eret
