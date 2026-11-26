    .text
    .align 2

    .global _vf3_gd_req_cmd
    .type _vf3_gd_req_cmd,@function
_vf3_gd_req_cmd:
    mov #0, r6
    mov #0, r7
    mov.l .Lvec_req, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_req:
    .long 0x8c0000bc

    .global _vf3_gd_get_cmd_stat
    .type _vf3_gd_get_cmd_stat,@function
_vf3_gd_get_cmd_stat:
    mov #0, r6
    mov #1, r7
    mov.l .Lvec_stat, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_stat:
    .long 0x8c0000bc

    .global _vf3_gd_exec_server
    .type _vf3_gd_exec_server,@function
_vf3_gd_exec_server:
    mov #0, r6
    mov #2, r7
    mov.l .Lvec_exec, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_exec:
    .long 0x8c0000bc

    .global _vf3_gd_init_system
    .type _vf3_gd_init_system,@function
_vf3_gd_init_system:
    mov #0, r6
    mov #3, r7
    mov.l .Lvec_init, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_init:
    .long 0x8c0000bc

    .global _vf3_gd_get_drv_stat
    .type _vf3_gd_get_drv_stat,@function
_vf3_gd_get_drv_stat:
    mov #0, r6
    mov #4, r7
    mov.l .Lvec_drv, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_drv:
    .long 0x8c0000bc

    .global _vf3_gd_g1_dma_end
    .type _vf3_gd_g1_dma_end,@function
_vf3_gd_g1_dma_end:
    mov #0, r6
    mov #5, r7
    mov.l .Lvec_g1, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_g1:
    .long 0x8c0000bc

    .global _vf3_gd_req_dma_trans
    .type _vf3_gd_req_dma_trans,@function
_vf3_gd_req_dma_trans:
    mov #0, r6
    mov #6, r7
    mov.l .Lvec_reqdma, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_reqdma:
    .long 0x8c0000bc

    .global _vf3_gd_check_dma_trans
    .type _vf3_gd_check_dma_trans,@function
_vf3_gd_check_dma_trans:
    mov #0, r6
    mov #7, r7
    mov.l .Lvec_checkdma, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_checkdma:
    .long 0x8c0000bc

    .global _vf3_gd_read_abort
    .type _vf3_gd_read_abort,@function
_vf3_gd_read_abort:
    mov #0, r6
    mov #8, r7
    mov.l .Lvec_abort, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_abort:
    .long 0x8c0000bc

    .global _vf3_gd_reset
    .type _vf3_gd_reset,@function
_vf3_gd_reset:
    mov #0, r6
    mov #9, r7
    mov.l .Lvec_reset, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_reset:
    .long 0x8c0000bc

    .global _vf3_gd_change_data_type
    .type _vf3_gd_change_data_type,@function
_vf3_gd_change_data_type:
    mov #0, r6
    mov #10, r7
    mov.l .Lvec_type, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_type:
    .long 0x8c0000bc

    .global _vf3_gd_set_pio_callback
    .type _vf3_gd_set_pio_callback,@function
_vf3_gd_set_pio_callback:
    mov #0, r6
    mov #11, r7
    mov.l .Lvec_piocb, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_piocb:
    .long 0x8c0000bc

    .global _vf3_gd_req_pio_trans
    .type _vf3_gd_req_pio_trans,@function
_vf3_gd_req_pio_trans:
    mov #0, r6
    mov #12, r7
    mov.l .Lvec_reqpio, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_reqpio:
    .long 0x8c0000bc

    .global _vf3_gd_check_pio_trans
    .type _vf3_gd_check_pio_trans,@function
_vf3_gd_check_pio_trans:
    mov #0, r6
    mov #13, r7
    mov.l .Lvec_checkpio, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
.Lvec_checkpio:
    .long 0x8c0000bc
