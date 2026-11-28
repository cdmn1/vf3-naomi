    .section .vf3_resident_text,"ax"
    .align 2
    .global _vf3_exception_stack_renderer_probe
    .type _vf3_exception_stack_renderer_probe,@function

_vf3_exception_stack_renderer_probe:
    /* Save caller state, then switch to the v174-validated BSS stack. */
    sts.l pr,@-r15
    mov.l r8,@-r15
    add #-8,r15
    mov r15,r8
    mov r4,r15

    /* Invoke only the resident draw-once renderer; no privileged reads. */
    mov.l .Lrenderer_draw,r0
    jsr @r0
    nop

    /* Restore caller stack and return a fixed token. */
    mov.l .Lrenderer_pass,r0
    mov r8,r15
    add #8,r15
    mov.l @r15+,r8
    lds.l @r15+,pr
    rts
    nop

    .align 4
.Lrenderer_draw: .long _vf3_diag_exception_draw_once
.Lrenderer_pass: .long 0x6ca7cafe
    .size _vf3_exception_stack_renderer_probe,.-_vf3_exception_stack_renderer_probe
