// EL0 test program: write, mmap, write from mapped memory, munmap, exit.
// Syscall ABI: number in x8, arguments in x0..x2, result in x0.

    .section .text
    .global _start
_start:
    mov  x0, #1
    adr  x1, message
    mov  x2, #15
    mov  x8, #1
    svc  #0

    // buffer = mmap(4096)
    mov  x0, #4096
    mov  x1, #0
    mov  x8, #5
    svc  #0
    mov  x19, x0

    // Copy the second message into the mapped page.
    adr  x20, mmap_message
    mov  x21, x19
    mov  x22, #8
copy_loop:
    cbz  x22, copy_done
    ldrb w23, [x20], #1
    strb w23, [x21], #1
    sub  x22, x22, #1
    b    copy_loop
copy_done:

    mov  x0, #1
    mov  x1, x19
    mov  x2, #8
    mov  x8, #1
    svc  #0

    mov  x0, x19
    mov  x1, #4096
    mov  x8, #6
    svc  #0

    mov  x0, #0
    mov  x8, #3
    svc  #0

1:  b    1b

message:
    .ascii "hello from el0\n"
mmap_message:
    .ascii "mmap ok\n"
