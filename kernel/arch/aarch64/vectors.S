// Exception vector table and trap entry stub.
// 16 slots, 128 bytes each, table aligned to 2048 as required by VBAR_EL1.

    .section .text
    .global g_vector_table
    .balign 2048
g_vector_table:
    .balign 128
    mov x0, #0
    b   OgTrapEntry
    .balign 128
    mov x0, #1
    b   OgTrapEntry
    .balign 128
    mov x0, #2
    b   OgTrapEntry
    .balign 128
    mov x0, #3
    b   OgTrapEntry
    .balign 128
    mov x0, #4
    b   OgTrapEntry
    .balign 128
    mov x0, #5
    b   OgTrapEntry
    .balign 128
    mov x0, #6
    b   OgTrapEntry
    .balign 128
    mov x0, #7
    b   OgTrapEntry
    .balign 128
    mov x0, #8
    b   OgTrapEntry
    .balign 128
    mov x0, #9
    b   OgTrapEntry
    .balign 128
    mov x0, #10
    b   OgTrapEntry
    .balign 128
    mov x0, #11
    b   OgTrapEntry
    .balign 128
    mov x0, #12
    b   OgTrapEntry
    .balign 128
    mov x0, #13
    b   OgTrapEntry
    .balign 128
    mov x0, #14
    b   OgTrapEntry
    .balign 128
    mov x0, #15
    b   OgTrapEntry

// Saves the full register state, calls the C++ dispatcher with
// (TrapFrame*, vector), restores, and returns from the exception.
OgTrapEntry:
    sub  sp, sp, #272
    stp  x0, x1, [sp, #0]
    stp  x2, x3, [sp, #16]
    stp  x4, x5, [sp, #32]
    stp  x6, x7, [sp, #48]
    stp  x8, x9, [sp, #64]
    stp  x10, x11, [sp, #80]
    stp  x12, x13, [sp, #96]
    stp  x14, x15, [sp, #112]
    stp  x16, x17, [sp, #128]
    stp  x18, x19, [sp, #144]
    stp  x20, x21, [sp, #160]
    stp  x22, x23, [sp, #176]
    stp  x24, x25, [sp, #192]
    stp  x26, x27, [sp, #208]
    stp  x28, x29, [sp, #224]
    str  x30, [sp, #240]
    mrs  x1, elr_el1
    mrs  x2, spsr_el1
    stp  x1, x2, [sp, #248]

    mov  x1, x0
    mov  x0, sp
    bl   OgTrapDispatch

    ldp  x1, x2, [sp, #248]
    msr  elr_el1, x1
    msr  spsr_el1, x2
    ldp  x0, x1, [sp, #0]
    ldp  x2, x3, [sp, #16]
    ldp  x4, x5, [sp, #32]
    ldp  x6, x7, [sp, #48]
    ldp  x8, x9, [sp, #64]
    ldp  x10, x11, [sp, #80]
    ldp  x12, x13, [sp, #96]
    ldp  x14, x15, [sp, #112]
    ldp  x16, x17, [sp, #128]
    ldp  x18, x19, [sp, #144]
    ldp  x20, x21, [sp, #160]
    ldp  x22, x23, [sp, #176]
    ldp  x24, x25, [sp, #192]
    ldp  x26, x27, [sp, #208]
    ldp  x28, x29, [sp, #224]
    ldr  x30, [sp, #240]
    add  sp, sp, #272
    eret
