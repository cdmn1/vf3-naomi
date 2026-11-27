    .section .vf3_resident_text,"ax"
    .align 2
    .macro VF3_STAGE value
    mov.l .Ldiag_stage,r0
    mov #\value,r4
    jsr @r0
    nop
    .endm
    .global vf3_gd_chainload
    .type vf3_gd_chainload,@function

/*
 * Enter the final handoff through the uncached P2 alias. This avoids executing
 * the transition code through KOS's cached P1 mapping while we invalidate and
 * disable the SH-4 caches.
 */
vf3_gd_chainload:
    mov.l .Lchainload_p2_addr,r0
    jmp @r0
    nop

.Lchainload_p2:
    /* The KOS stack is no longer used after this point. */
    mov.l .Lstack,r15
    /*
     * Skip the explicit OCBI sweep. The later CCR write requests whole-cache
     * O/I invalidation, so a per-line destination sweep is redundant here.
     * This checkpoint now leads directly into the uncached image copy.
     */
    VF3_STAGE 2

    /* Copy the exact 1,179,648-byte retail image via uncached P2 aliases. */
    mov.l .Lsource_p2,r0
    mov.l .Ldest_p2,r1
    mov.l .Lword_count,r2
.Lcopy:
    mov.l @r0+,r3
    mov.l r3,@r1
    add #4,r1
    dt r2
    bf .Lcopy
    VF3_STAGE 5

    /*
     * Match the cache invalidation used by Dreamcast IP bootstraps.
     * CCR 0x808 asserts both OCI and ICI; it invalidates the operand and
     * instruction caches but does not clear the OCE/ICE enable bits. This
     * routine is executing from P2 while the cache arrays are invalidated.
     */
    mov.l .Lccr_addr,r0
    mov.l .Lccr_value,r1
    mov.l r1,@r0
    nop
    nop
    VF3_STAGE 6

    /* Establish a clean Katana startup CPU context. */
    mov.l .Lvbr,r0
    ldc r0,vbr
    mov.l .Lgbr,r0
    ldc r0,gbr
    mov.l .Lfpscr,r0
    lds r0,fpscr
    mov.l .Lsr,r0
    ldc r0,sr

    /* Clear scratch/callee registers; r4 carries the retail entry address. */
    mov #0,r1
    mov r1,r2
    mov r1,r3
    mov r1,r5
    mov r1,r6
    mov r1,r7
    mov r1,r8
    mov r1,r9
    mov r1,r10
    mov r1,r11
    mov r1,r12
    mov r1,r13
    mov r1,r14
    VF3_STAGE 7
    /* v108 diagnostic: force a TRAPA after handoff setup to validate the
       installed SH-4 exception vector. This is intentionally not a game boot. */
    trapa #0x3f
    nop
    mov.l .Lentry,r0
    jsr @r0
    mov r0,r4
.Lreturned:
    bra .Lreturned
    nop

    .align 4
.Ldiag_stage:  .long _vf3_diag_begin
.Lchainload_p2_addr: .long 0xad000000 + (.Lchainload_p2 - vf3_gd_chainload)
.Lsource_p2:    .long 0xad100000
.Ldest_p1:      .long 0x8c010000
.Ldest_p2:      .long 0xac010000
.Lend_p1:       .long 0x8c130000
.Lword_count:   .long 294912
.Lstack:        .long 0xac00fc00
.Lvbr:          .long 0x8c00f400
.Lgbr:          .long 0x00000000
.Lfpscr:        .long 0x00040001
.Lsr:           .long 0x600000f0
.Lccr_addr:     .long 0xff00001c
.Lccr_value:    .long 0x00000808
.Lentry:        .long 0x8c010000

    .size vf3_gd_chainload,.-vf3_gd_chainload
