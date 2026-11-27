    .section .vf3_resident_text,"ax"
    .align 2
    .macro VF3_STAGE value, diagptr=.Ldiag_stage
    mov.l \diagptr,r0
    mov #\value,r4
    jsr @r0
    nop
    .endm
    .global vf3_gd_chainload
    .type vf3_gd_chainload,@function

vf3_gd_chainload:
    /* Checkpoint A: prove the C wrapper entered the resident assembly. */
    VF3_STAGE 1, .Ldiag_stage
    mov.l .Lchainload_p2_addr,r0
    jmp @r0
    nop

.Lchainload_p2:
    /*
     * Checkpoint B: write a visible red marker directly through P2 MMIO/VRAM
     * before calling the P1 diagnostic routine. The marker is independent of
     * that C call and therefore proves the P2 instruction stream was reached.
     */
    mov.l .Lfbreg_addr,r0
    mov.l @r0,r1
    mov.l .Lfb_mask,r2
    and r2,r1
    mov.l .Lvram_base,r2
    or r2,r1
    mov.l .Lfb_cfg_addr,r0
    mov.l @r0,r3
    shlr2 r3
    mov r3,r0
    and #3,r0
    mov r0,r3
    tst r3,r3
    bf .Lmark_565
    mov.l .Lred_1555,r4
    bra .Lmark_color_ready
    nop
.Lmark_565:
    mov.l .Lred_565,r4
.Lmark_color_ready:
    mov #128,r5
.Lmark_loop:
    mov.w r4,@r1
    add #2,r1
    dt r5
    bf .Lmark_loop

    /* This display update is secondary; the red VRAM stripe is the proof. */
    VF3_STAGE 2, .Ldiag_stage
    mov.l .Lstack,r15

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

    /* Match the cache invalidation used by Dreamcast IP bootstraps. */
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
    trapa #0x3f
    nop
    mov.l .Lentry,r0
    jsr @r0
    mov r0,r4
.Lreturned:
    bra .Lreturned
    nop

    .align 4
.Ldiag_stage:        .long _vf3_diag_begin
.Lchainload_p2_addr: .long 0xad000000 + (.Lchainload_p2 - vf3_gd_chainload)
.Lfbreg_addr:        .long 0xa05f8050
.Lfb_cfg_addr:       .long 0xa05f8044
.Lfb_mask:           .long 0x007fffff
.Lvram_base:         .long 0xa5000000
.Lred_1555:          .long 0x00007c00
.Lred_565:           .long 0x0000f800
.Lsource_p2:         .long 0xad100000
.Ldest_p2:           .long 0xac010000
.Lword_count:        .long 294912
.Lstack:             .long 0xac00fc00
.Lvbr:               .long 0x8c00f400
.Lgbr:               .long 0x00000000
.Lfpscr:             .long 0x00040001
.Lsr:                .long 0x600000f0
.Lccr_addr:          .long 0xff00001c
.Lccr_value:         .long 0x00000808
.Lentry:             .long 0x8c010000

    .size vf3_gd_chainload,.-vf3_gd_chainload
