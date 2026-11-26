    .section .vf3_resident_text,"ax"
    .align 2
    .global vf3_gd_chainload
    .type vf3_gd_chainload,@function
vf3_gd_chainload:
    /* Discard cached KOS lines covering the destination before overwriting
       the low-memory application image through the uncached P2 alias. */
    mov.l .Ldest_p1,r0
    mov.l .Lend_p1,r1
.Ldcache:
    ocbi @r0
    add #32,r0
    cmp/hs r1,r0
    bf .Ldcache

    /* Copy 1,179,648 verified bytes: P2 staging -> P2 retail load address. */
    mov.l .Lsource_p2,r0
    mov.l .Ldest_p2,r1
    mov.l .Lword_count,r2
.Lcopy:
    mov.l @r0+,r3
    mov.l r3,@r1
    add #4,r1
    dt r2
    bf .Lcopy

    /* Make the newly copied instructions visible to the SH-4 instruction
       fetch path before transferring control. */
    mov.l .Ldest_p1,r0
    mov.l .Lend_p1,r1
.Licache:
    icbi @r0
    add #32,r0
    cmp/hs r1,r0
    bf .Licache
    nop

    /* Mask interrupts while changing execution context. The resident bridge
       remains in 0x8Dxxxxxx and retains the GD-ROM service vector. */
    stc sr,r0
    or #0xf0,r0
    ldc r0,sr
    mov.l .Lstack,r15
    mov.l .Lentry,r0
    jmp @r0
    nop
    .align 2
.Lsource_p2: .long 0xad100000
.Ldest_p1:   .long 0x8c010000
.Ldest_p2:   .long 0xac010000
.Lend_p1:    .long 0x8c130000
.Lword_count:.long 294912
.Lstack:     .long 0x8dfff000
.Lentry:     .long 0x8c010000
    .size vf3_gd_chainload,.-vf3_gd_chainload
