    .section .vf3_resident_text,"ax"
    .align 2
    .global _vf3_exception_entry
    .type _vf3_exception_entry,@function

_vf3_exception_entry:
    /* Switch to a stack reserved outside the retail executable and IP area. */
    mov.l .Lexc_stack,r15
    mov.l .Lexc_info,r1
    stc spc,r0
    mov.l r0,@r1
    stc ssr,r0
    mov.l r0,@(4,r1)
    stc sgr,r0
    mov.l r0,@(8,r1)
    mov.l .Lexpevt,r2
    mov.l @r2,r0
    mov.l r0,@(12,r1)
    mov.l .Ltea,r2
    mov.l @r2,r0
    mov.l r0,@(16,r1)
    stc sr,r0
    mov.l r0,@(20,r1)
    mov.l .Lexc_display,r0
    jsr @r0
    nop
.Lexc_loop:
    bra .Lexc_loop
    nop
    .align 4
.Lexc_stack:   .long 0x8d0f0000
.Lexc_info:    .long _vf3_exception_info
.Lexpevt:      .long 0xff000024
.Ltea:         .long 0xff00000c
.Lexc_display: .long _vf3_diag_exception_display
    .size _vf3_exception_entry,.-_vf3_exception_entry
