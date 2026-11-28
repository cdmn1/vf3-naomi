    .section .vf3_resident_text,"ax"
    .align 2
    .global _vf3_exception_stack_probe
    .type _vf3_exception_stack_probe,@function

_vf3_exception_stack_probe:
    /* Establish both SH-4 stack and frame-pointer registers on the new stack. */
    mov r4,r15
    mov r15,r14
    /* Verify the supplied stack is writable before entering compiled C. */
    mov.l .Lsentinel,r0
    mov.l r0,@-r15
    mov.l @r15,r1
    cmp/eq r0,r1
    bf .Lstack_bad
    add #4,r15
    mov.l .Ldisplay,r0
    jsr @r0
    nop
.Lstack_loop:
    bra .Lstack_loop
    nop
.Lstack_bad:
    bra .Lstack_bad
    nop
    .align 4
.Lsentinel: .long 0x51ac0ffe
.Ldisplay: .long _vf3_diag_exception_display
    .size _vf3_exception_stack_probe,.-_vf3_exception_stack_probe
