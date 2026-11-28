    .section .vf3_resident_text,"ax"
    .align 2
    .global _vf3_exception_capture_return_probe
    .type _vf3_exception_capture_return_probe,@function

_vf3_exception_capture_return_probe:
    /* Save caller state before switching to the v174-validated BSS stack. */
    sts.l pr,@-r15
    mov.l r8,@-r15
    add #-8,r15
    mov r15,r8
    mov r4,r15

    /* Capture the same privileged state as the exception-entry routine. */
    mov.l .Lcapture_info,r1
    stc spc,r0
    mov.l r0,@r1
    stc ssr,r0
    mov.l r0,@(4,r1)
    stc sgr,r0
    mov.l r0,@(8,r1)
    mov.l .Lcapture_expevt,r2
    mov.l @r2,r0
    mov.l r0,@(12,r1)
    mov.l .Lcapture_tea,r2
    mov.l @r2,r0
    mov.l r0,@(16,r1)
    stc sr,r0
    mov.l r0,@(20,r1)

    /* Return a fixed success token without calling resident C or rendering. */
    mov.l .Lcapture_pass,r0
    mov r8,r15
    add #8,r15
    mov.l @r15+,r8
    lds.l @r15+,pr
    rts
    nop

    .align 4
.Lcapture_info:   .long _vf3_exception_info
.Lcapture_expevt: .long 0xff000024
.Lcapture_tea:    .long 0xff00000c
.Lcapture_pass:   .long 0x6ca7cafe
    .size _vf3_exception_capture_return_probe,.-_vf3_exception_capture_return_probe
