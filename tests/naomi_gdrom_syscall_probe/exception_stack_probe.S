    .section .vf3_resident_text,"ax"
    .align 2
    .global _vf3_exception_stack_probe
    .type _vf3_exception_stack_probe,@function

_vf3_exception_stack_probe:
    /* Preserve the caller's callee-saved r8 and original stack, aligned. */
    mov.l r8,@-r15
    add #-12,r15
    mov r15,r8
    /* Switch stacks without changing r14; test a real C call on this stack. */
    mov r4,r15
    mov.l .Lsentinel,r0
    mov.l r0,@-r15
    mov.l @r15,r1
    cmp/eq r0,r1
    bf .Lstack_bad
    add #4,r15
    mov.l .Ldraw_once,r0
    jsr @r0
    nop
    /* Return to the original KOS stack and return to the caller. */
    mov r8,r15
    add #12,r15
    mov.l @r15+,r8
    rts
    nop
.Lstack_bad:
    bra .Lstack_bad
    nop
    .align 4
.Lsentinel: .long 0x51ac0ffe
.Ldraw_once: .long _vf3_diag_exception_draw_once
    .size _vf3_exception_stack_probe,.-_vf3_exception_stack_probe
