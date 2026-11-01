// Minimal EL0 program: write a message, then exit.
// Syscall ABI: number in x8, arguments in x0..x2, result in x0.

    .section .text
    .global _start
_start:
    mov  x0, #1
    adr  x1, message
    mov  x2, #15
    mov  x8, #1
    svc  #0

    mov  x0, #0
    mov  x8, #3
    svc  #0

1:  b    1b

message:
    .ascii "hello from el0\n"
