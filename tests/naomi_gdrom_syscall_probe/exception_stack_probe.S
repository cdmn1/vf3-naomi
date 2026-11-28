    .section .vf3_resident_text,"ax"
    .align 2
    .global _vf3_exception_stack_probe
    .type _vf3_exception_stack_probe,@function

_vf3_exception_stack_probe:
    /* Isolate the dedicated exception-stack switch from register capture. */
    mov.l .Lstack,r15
    mov.l .Ldisplay,r0
    jsr @r0
    nop
.Lstack_loop:
    bra .Lstack_loop
    nop
    .align 4
.Lstack:   .long 0x8d0f0000
.Ldisplay: .long _vf3_diag_exception_display
    .size _vf3_exception_stack_probe,.-_vf3_exception_stack_probe
