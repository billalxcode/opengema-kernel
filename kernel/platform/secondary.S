// Secondary core entry. PSCI enters here with MMU off and x0 holding the
// context id, which is the physical stack top.

    .section .text
    .global SecondaryEntry
SecondaryEntry:
    mov  sp, x0
    bl   SecondaryMain
1:  wfi
    b    1b
