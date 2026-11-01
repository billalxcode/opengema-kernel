// Exception vector table and trap entry stub.
// 16 slots, 128 bytes each, table aligned to 2048 as required by VBAR_EL1.
//
// Each slot stashes x0 before using it, so the user's first argument is not
// replaced by the vector number.

    .section .text
    .global g_vector_table
    .balign 2048
g_vector_table:
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #0
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #1
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #2
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #3
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #4
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #5
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #6
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #7
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #8
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #9
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #10
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #11
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #12
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #13
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #14
    b   OgTrapEntry
    .balign 128
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #15
    b   OgTrapEntry

// Frame layout matches TrapFrame: x[0..30], elr, spsr.
// The stashed x0 lives 16 bytes above the 272 byte allocation.
OgTrapEntry:
    sub  sp, sp, #272
    stp  x1, x2, [sp, #8]
    stp  x3, x4, [sp, #24]
    stp  x5, x6, [sp, #40]
    stp  x7, x8, [sp, #56]
    stp  x9, x10, [sp, #72]
    stp  x11, x12, [sp, #88]
    stp  x13, x14, [sp, #104]
    stp  x15, x16, [sp, #120]
    stp  x17, x18, [sp, #136]
    stp  x19, x20, [sp, #152]
    stp  x21, x22, [sp, #168]
    stp  x23, x24, [sp, #184]
    stp  x25, x26, [sp, #200]
    stp  x27, x28, [sp, #216]
    stp  x29, x30, [sp, #232]

    ldr  x1, [sp, #272]
    str  x1, [sp, #0]

    mrs  x1, elr_el1
    mrs  x2, spsr_el1
    stp  x1, x2, [sp, #248]

    mov  x1, x0
    mov  x0, sp
    bl   OgTrapDispatch

    ldp  x1, x2, [sp, #248]
    msr  elr_el1, x1
    msr  spsr_el1, x2
    ldr  x0, [sp, #0]
    ldp  x1, x2, [sp, #8]
    ldp  x3, x4, [sp, #24]
    ldp  x5, x6, [sp, #40]
    ldp  x7, x8, [sp, #56]
    ldp  x9, x10, [sp, #72]
    ldp  x11, x12, [sp, #88]
    ldp  x13, x14, [sp, #104]
    ldp  x15, x16, [sp, #120]
    ldp  x17, x18, [sp, #136]
    ldp  x19, x20, [sp, #152]
    ldp  x21, x22, [sp, #168]
    ldp  x23, x24, [sp, #184]
    ldp  x25, x26, [sp, #200]
    ldp  x27, x28, [sp, #216]
    ldp  x29, x30, [sp, #232]
    add  sp, sp, #288
    eret
