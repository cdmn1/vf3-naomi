    .text
    .align 2
    .macro GDSTUB name, op
    .global \name
    .type \name,@function
\name:
    mov #0, r6
    mov #\op, r7
    mov.l 1f, r0
    mov.l @r0, r0
    jmp @r0
    nop
    .align 2
1:
    .long 0x8c0000bc
    .endm
    GDSTUB vf3_gd_req_cmd, 0
    GDSTUB vf3_gd_get_cmd_stat, 1
    GDSTUB vf3_gd_exec_server, 2
    GDSTUB vf3_gd_init_system, 3
    GDSTUB vf3_gd_get_drv_stat, 4
    GDSTUB vf3_gd_g1_dma_end, 5
    GDSTUB vf3_gd_req_dma_trans, 6
    GDSTUB vf3_gd_check_dma_trans, 7
    GDSTUB vf3_gd_read_abort, 8
    GDSTUB vf3_gd_reset, 9
    GDSTUB vf3_gd_change_data_type, 10
    GDSTUB vf3_gd_set_pio_callback, 11
    GDSTUB vf3_gd_req_pio_trans, 12
    GDSTUB vf3_gd_check_pio_trans, 13
