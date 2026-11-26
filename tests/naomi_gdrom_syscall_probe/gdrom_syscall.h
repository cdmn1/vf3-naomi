#ifndef VF3_NAOMI_GDROM_SYSCALL_H
#define VF3_NAOMI_GDROM_SYSCALL_H
#include <stdint.h>
#define VF3_GD_FAD_BASE 45150u
#define VF3_GD_SECTOR_BYTES 2048u
#define VF3_GD_CMD_PIOREAD 0x10u
#define VF3_GD_CMD_DMAREAD 0x11u
enum {
 VF3_GD_REQ_CMD=0, VF3_GD_GET_CMD_STAT=1, VF3_GD_EXEC_SERVER=2,
 VF3_GD_INIT_SYSTEM=3, VF3_GD_GET_DRV_STAT=4, VF3_GD_G1_DMA_END=5,
 VF3_GD_REQ_DMA_TRANS=6, VF3_GD_CHECK_DMA_TRANS=7, VF3_GD_READ_ABORT=8,
 VF3_GD_RESET=9, VF3_GD_CHANGE_DATA_TYPE=10, VF3_GD_SET_PIO_CALLBACK=11,
 VF3_GD_REQ_PIO_TRANS=12, VF3_GD_CHECK_PIO_TRANS=13
};
enum { VF3_GDC_ERR=-1, VF3_GDC_OK=0, VF3_GDC_BUSY=1, VF3_GDC_COMPLETE=2, VF3_GDC_CONTINUE=3 };
uint32_t vf3_gd_install(void);
uint32_t vf3_gd_original_service(void);
uint32_t vf3_gd_last_dma_status(void);
uint32_t vf3_gd_last_bytes(void);
uint32_t vf3_gd_req_cmd(uint32_t command, const uint32_t *params);
uint32_t vf3_gd_get_cmd_stat(uint32_t request_id, uint32_t *ext);
uint32_t vf3_gd_exec_server(void);
uint32_t vf3_gd_init_system(void);
uint32_t vf3_gd_get_drv_stat(uint32_t *out);
uint32_t vf3_gd_g1_dma_end(uint32_t a, uint32_t b);
uint32_t vf3_gd_req_dma_trans(uint32_t request_id, uint32_t *remaining);
uint32_t vf3_gd_check_dma_trans(uint32_t request_id, uint32_t *remaining);
uint32_t vf3_gd_read_abort(uint32_t request_id);
uint32_t vf3_gd_reset(void);
uint32_t vf3_gd_change_data_type(uint32_t type);
uint32_t vf3_gd_set_pio_callback(uint32_t callback);
uint32_t vf3_gd_req_pio_trans(uint32_t request_id, uint32_t *remaining);
uint32_t vf3_gd_check_pio_trans(uint32_t request_id, uint32_t *remaining);
#endif
