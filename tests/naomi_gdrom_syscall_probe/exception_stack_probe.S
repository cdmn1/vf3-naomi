    .section .vf3_resident_text,"ax"
    .align 2
    .global _vf3_exception_stack_probe
    .type _vf3_exception_stack_probe,@function

_vf3_exception_stack_probe:
    /* r4 is a caller-supplied top-of-stack in KOS-allocated RAM. */
    mov r4,r15
    mov.l .Ldisplay,r0
    jsr @r0
    nop
.Lstack_loop:
    bra .Lstack_loop
    nop
    .align 4
.Ldisplay: .long _vf3_diag_exception_display
    .size _vf3_exception_stack_probe,.-_vf3_exception_stack_probe
