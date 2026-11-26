#include <stdint.h>
#include <stdio.h>
#include "gdrom_syscall.h"

#define PARAMS_ADDR 0x0c000100u
#define REMAIN_ADDR 0x0c000200u
#define RESULT_ADDR 0x0c000300u
#define DEST_ADDR   0x0c010000u

static int check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static uint32_t begin_read(void)
{
    volatile uint32_t *p = vf3_gd_host_words(PARAMS_ADDR);
    p[0] = VF3_GD_FAD_BASE;
    p[1] = 1u;
    p[2] = DEST_ADDR;
    p[3] = 0u;
    return vf3_gd_host_call(VF3_GD_CMD_DMAREAD, PARAMS_ADDR, 0u,
                            VF3_GD_REQ_CMD);
}

int main(void)
{
    volatile uint32_t *remaining, *result;
    uint32_t request, status;
    int ok = 1;

    vf3_gd_host_reset();
    remaining = vf3_gd_host_words(REMAIN_ADDR);
    result = vf3_gd_host_words(RESULT_ADDR);
    request = begin_read();
    ok &= check(request != 0u, "DMAREAD request is accepted");
    ok &= check(vf3_gd_host_call(request, REMAIN_ADDR, 0u,
                                 VF3_GD_REQ_DMA_TRANS) == 0u,
                "REQ_DMA_TRANS accepts the active request");
    ok &= check(*remaining == 1u, "REQ_DMA_TRANS reports one remaining sector");
    ok &= check(vf3_gd_host_call(0u, 0u, 0u, VF3_GD_EXEC_SERVER) == 0u,
                "EXEC_SERVER starts the NetDIMM DMA");
    ok &= check(begin_read() == 0u,
                "a second request is rejected while the first is pending");

    vf3_gd_host_complete_dma();
    status = vf3_gd_host_call(request, RESULT_ADDR, 0u, VF3_GD_GET_CMD_STAT);
    ok &= check(status == VF3_GDC_COMPLETE,
                "GET_CMD_STAT reports completion on the poll that observes DMA end");
    ok &= check(result[2] == VF3_GD_SECTOR_BYTES,
                "terminal GET_CMD_STAT reports the final transferred byte count");

    /* A terminal status is retained until the matching request ID is queried. */
    vf3_gd_host_reset();
    remaining = vf3_gd_host_words(REMAIN_ADDR);
    result = vf3_gd_host_words(RESULT_ADDR);
    request = begin_read();
    ok &= check(request != 0u, "second DMAREAD request is accepted");
    vf3_gd_host_call(0u, 0u, 0u, VF3_GD_EXEC_SERVER);
    vf3_gd_host_complete_dma();
    status = vf3_gd_host_call(request + 1u, RESULT_ADDR, 0u,
                              VF3_GD_GET_CMD_STAT);
    ok &= check(status == VF3_GDC_OK,
                "a mismatched request ID does not consume terminal status");
    ok &= check(begin_read() == 0u,
                "a new request is rejected until terminal status is consumed");
    status = vf3_gd_host_call(request, RESULT_ADDR, 0u, VF3_GD_GET_CMD_STAT);
    ok &= check(status == VF3_GDC_COMPLETE && result[2] == VF3_GD_SECTOR_BYTES,
                "the matching request can still retrieve completion and byte count");

    if (!ok) return 1;
    puts("GD-ROM bridge host contract: PASS");
    puts("REQ_CMD gating: PASS");
    puts("GET_CMD_STAT terminal byte count: PASS");
    puts("request-ID completion ownership: PASS");
    return 0;
}
