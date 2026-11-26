#include <kos.h>
#include <stdint.h>
#include <string.h>
#include "gdrom_syscall.h"

#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define NAOMI_DMA_OFFSETH 0x005f700cu
#define NAOMI_DMA_OFFSETL 0x005f7010u
#define SB_GDSTAR 0x005f7404u
#define SB_GDLEN 0x005f7408u
#define SB_GDDIR 0x005f740cu
#define SB_GDEN 0x005f7414u
#define SB_GDST 0x005f7418u
#define MAX_TRANSFER_BYTES (1024u * 1024u)

typedef struct {
    uint32_t request_id, next_request_id, command, params[4];
    uint32_t total_bytes, transferred_bytes, dma_status, status, started;
} vf3_gd_state;

static volatile uint32_t *const service_vector =
    (volatile uint32_t *)(uintptr_t)0x8c0000bcu;
static uint32_t saved_service;
static vf3_gd_state gd;

static void invalidate_destination(uint32_t dest, uint32_t bytes)
{
    uint32_t physical = dest & 0x1fffffffu;
    uintptr_t p = (uintptr_t)(0x8c000000u | physical) & ~(uintptr_t)31u;
    uintptr_t end = ((uintptr_t)(0x8c000000u | physical) + bytes + 31u) & ~(uintptr_t)31u;
    for (; p < end; p += 32u)
        __asm__ volatile("ocbi @%0" : : "r"(p) : "memory");
}

static int start_cart_dma(void)
{
    uint64_t source_offset, byte_count;
    uint32_t physical;
    if (gd.params[0] < VF3_GD_FAD_BASE || gd.params[1] == 0)
        return -1;
    source_offset = (uint64_t)(gd.params[0] - VF3_GD_FAD_BASE) * VF3_GD_SECTOR_BYTES;
    byte_count = (uint64_t)gd.params[1] * VF3_GD_SECTOR_BYTES;
    if (source_offset > 0xffffffffu || byte_count > MAX_TRANSFER_BYTES || byte_count > 0xffffffffu)
        return -2;
    physical = gd.params[2] & 0x1fffffffu;
    if (physical < 0x01000000u || physical + (uint32_t)byte_count < physical)
        return -3;

    invalidate_destination(gd.params[2], (uint32_t)byte_count);
    REG16(NAOMI_DMA_OFFSETH) = (uint16_t)(source_offset >> 16);
    REG16(NAOMI_DMA_OFFSETL) = (uint16_t)source_offset;
    REG32(SB_GDSTAR) = physical;
    REG32(SB_GDLEN) = (uint32_t)byte_count;
    REG32(SB_GDDIR) = 1u;
    REG32(SB_GDEN) = 1u;
    REG32(SB_GDST) = 1u;
    gd.total_bytes = (uint32_t)byte_count;
    gd.transferred_bytes = 0;
    gd.started = 1u;
    gd.status = VF3_GDC_BUSY;
    return 0;
}

static void poll_cart_dma(void)
{
    if (gd.status != VF3_GDC_BUSY || !gd.started)
        return;
    gd.dma_status = REG32(SB_GDST);
    if ((gd.dma_status & 1u) == 0) {
        REG32(SB_GDEN) = 0;
        gd.transferred_bytes = gd.total_bytes;
        gd.started = 0;
        gd.status = VF3_GDC_COMPLETE;
    }
}

static uint32_t remaining_sectors(void)
{
    return gd.status == VF3_GDC_COMPLETE ? 0u : gd.params[1];
}

static uint32_t vf3_gd_syscall_handler(uint32_t r4, uint32_t r5,
                                       uint32_t r6, uint32_t r7)
{
    volatile uint32_t *words;
    if (r6 != 0u) {
        /* The two r6=-1 variants are not GD-ROM operations. Keep them harmless
           in this probe; production will preserve any required Naomi service. */
        if (r6 == 0xffffffffu && (r7 == 0u || r7 == 1u))
            return 0u;
        return (uint32_t)-1;
    }

    switch (r7) {
    case VF3_GD_REQ_CMD: {
        uint32_t next = gd.next_request_id ? gd.next_request_id : 2u;
        if (gd.status == VF3_GDC_BUSY)
            return 0u;
        memset(&gd, 0, sizeof gd);
        gd.next_request_id = next + 1u;
        gd.request_id = next;
        gd.command = r4;
        gd.status = VF3_GDC_BUSY;
        if (r5) {
            words = (volatile uint32_t *)(uintptr_t)r5;
            for (unsigned i = 0; i < 4; ++i)
                gd.params[i] = words[i];
        }
        return gd.request_id;
    }

    case VF3_GD_GET_CMD_STAT: {
        uint32_t result[4] = {0, 0, gd.transferred_bytes, 0};
        uint32_t ret;
        poll_cart_dma();
        ret = gd.status;
        if (r5) {
            words = (volatile uint32_t *)(uintptr_t)r5;
            for (unsigned i = 0; i < 4; ++i)
                words[i] = result[i];
        }
        if (ret == VF3_GDC_COMPLETE || ret == (uint32_t)VF3_GDC_ERR) {
            gd.status = VF3_GDC_OK;
            gd.request_id = 0xffffffffu;
        }
        return ret;
    }

    case VF3_GD_EXEC_SERVER:
        if (gd.status == VF3_GDC_BUSY && !gd.started) {
            if (gd.command == VF3_GD_CMD_DMAREAD || gd.command == VF3_GD_CMD_PIOREAD) {
                if (start_cart_dma() != 0)
                    gd.status = (uint32_t)VF3_GDC_ERR;
            } else {
                gd.status = VF3_GDC_COMPLETE;
            }
        }
        return 0u;

    case VF3_GD_INIT_SYSTEM:
    case VF3_GD_G1_DMA_END:
    case VF3_GD_CHANGE_DATA_TYPE:
    case VF3_GD_SET_PIO_CALLBACK:
        return 0u;

    case VF3_GD_GET_DRV_STAT:
        if (r4) {
            words = (volatile uint32_t *)(uintptr_t)r4;
            words[0] = 1u;
            words[1] = 0x80u;
        }
        return 0u;

    case VF3_GD_REQ_DMA_TRANS:
    case VF3_GD_REQ_PIO_TRANS:
        if (r4 != gd.request_id)
            return (uint32_t)-1;
        if (r5)
            *(volatile uint32_t *)(uintptr_t)r5 = remaining_sectors();
        return 0u;

    case VF3_GD_CHECK_DMA_TRANS:
    case VF3_GD_CHECK_PIO_TRANS:
        poll_cart_dma();
        if (r5)
            *(volatile uint32_t *)(uintptr_t)r5 = remaining_sectors();
        return gd.status == VF3_GDC_BUSY ? 0u : 1u;

    case VF3_GD_READ_ABORT:
        if (r4 != gd.request_id)
            return (uint32_t)-1;
        REG32(SB_GDEN) = 0u;
        gd.started = 0u;
        gd.status = VF3_GDC_OK;
        gd.request_id = 0xffffffffu;
        return 0u;

    case VF3_GD_RESET:
        REG32(SB_GDEN) = 0u;
        memset(&gd, 0, sizeof gd);
        gd.next_request_id = 2u;
        gd.status = VF3_GDC_OK;
        gd.request_id = 0xffffffffu;
        return 0u;

    default:
        return (uint32_t)-1;
    }
}

uint32_t vf3_gd_install(void)
{
    saved_service = *service_vector;
    *service_vector = (uint32_t)(uintptr_t)vf3_gd_syscall_handler;
    memset(&gd, 0, sizeof gd);
    gd.next_request_id = 2u;
    gd.request_id = 0xffffffffu;
    gd.status = VF3_GDC_OK;
    return saved_service;
}

uint32_t vf3_gd_original_service(void) { return saved_service; }

uint32_t vf3_gd_last_dma_status(void)
{
    poll_cart_dma();
    return gd.dma_status;
}

uint32_t vf3_gd_last_bytes(void)
{
    poll_cart_dma();
    return gd.transferred_bytes;
}
