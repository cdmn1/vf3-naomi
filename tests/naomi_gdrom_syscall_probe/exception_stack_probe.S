    .section .vf3_resident_text,"ax"
    .align 2
    .global _vf3_exception_stack_probe
    .type _vf3_exception_stack_probe,@function

_vf3_exception_stack_probe:
    /* Preserve caller r8 and original stack on the original KOS stack. */
    mov.l r8,@-r15
    add #-12,r15
    mov r15,r8

    /* Switch to caller-supplied aligned BSS stack and exercise a resident C frame. */
    mov r4,r15
    mov.l .Lsentinel,r0
    mov.l r0,@-r15
    mov.l @r15,r1
    cmp/eq r0,r1
    bf .Lstack_bad
    add #4,r15
    mov.l .Lstack_test,r0
    jsr @r0
    nop

    /* Preserve the C result in r0; restore the original KOS stack and r8. */
    mov r8,r15
    add #12,r15
    mov.l @r15+,r8
    rts
    nop

.Lstack_bad:
    mov.l .Lbad_result,r0
    mov r8,r15
    add #12,r15
    mov.l @r15+,r8
    rts
    nop

    .align 4
.Lsentinel:   .long 0x51ac0ffe
.Lbad_result: .long 0xbad00001
.Lstack_test: .long _vf3_diag_stack_c_test
    .size _vf3_exception_stack_probe,.-_vf3_exception_stack_probe
