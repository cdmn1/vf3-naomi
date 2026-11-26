#include <kos.h>
#include <stdint.h>
#include <string.h>
#include "gdrom_syscall.h"
#include "vf3_asset_map.h"

#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define NAOMI_DMA_OFFSETH 0x005f700cu
#define NAOMI_DMA_OFFSETL 0x005f7010u
#define SB_GDSTAR 0x005f7404u
#define SB_GDLEN 0x005f7408u
#define SB_GDDIR 0x005f740cu
#define SB_GDEN 0x005f7414u
#define SB_GDST 0x005f7418u
#define MAX_DMA_CHUNK_BYTES (1024u * 1024u)
#define MAX_REQUEST_BYTES (32u * 1024u * 1024u)

typedef struct {
    uint32_t request_id, next_request_id, command, params[4];
    uint32_t total_bytes, transferred_bytes, dma_status, status, started;
    uint32_t current_fad, remaining_sectors, current_dest, chunk_sectors, chunk_bytes;
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

static const vf3_asset_range *find_asset_range(uint32_t fad)
{
#if VF3_ASSET_MAP_COUNT > 0
    unsigned lo = 0, hi = VF3_ASSET_MAP_COUNT;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2u;
        const vf3_asset_range *e = &vf3_asset_map[mid];
        if (fad < e->fad_start) hi = mid;
        else if (fad - e->fad_start >= e->sectors) lo = mid + 1u;
        else return e;
    }
    return 0;
#else
    static const vf3_asset_range probe_range = {
        VF3_GD_FAD_BASE, 0xffffffffu - VF3_GD_FAD_BASE, 0u
    };
    return fad >= probe_range.fad_start ? &probe_range : 0;
#endif
}

/* Start one contiguous part of the guest request. A request can span several
   packed assets; each asset boundary is handled as a separate Holly DMA. */
static int start_cart_dma_chunk(void)
{
    const vf3_asset_range *entry;
    uint32_t sectors, bytes, source_offset, physical;
    uint64_t offset64;
    entry = find_asset_range(gd.current_fad);
    if (!entry || gd.remaining_sectors == 0u)
        return -1;

    sectors = entry->sectors - (gd.current_fad - entry->fad_start);
    if (sectors > gd.remaining_sectors) sectors = gd.remaining_sectors;
    if (sectors > MAX_DMA_CHUNK_BYTES / VF3_GD_SECTOR_BYTES)
        sectors = MAX_DMA_CHUNK_BYTES / VF3_GD_SECTOR_BYTES;
    if (sectors == 0u) return -1;

    /* Generated map entries store payload-relative offsets. Add the base
       exactly once to obtain the absolute NetDIMM cartridge byte address. */
    offset64 = (uint64_t)VF3_ASSET_PAYLOAD_BASE +
               (uint64_t)entry->payload_offset +
               (uint64_t)(gd.current_fad - entry->fad_start) * VF3_GD_SECTOR_BYTES;
#if VF3_ASSET_MAP_COUNT == 0
    offset64 = (uint64_t)(gd.current_fad - VF3_GD_FAD_BASE) * VF3_GD_SECTOR_BYTES;
#endif
    if (offset64 > 0xffffffffu) return -2;
    source_offset = (uint32_t)offset64;
    bytes = sectors * VF3_GD_SECTOR_BYTES;
    physical = gd.current_dest & 0x1fffffffu;
    if (physical < 0x01000000u || physical + bytes < physical)
        return -3;

    invalidate_destination(gd.current_dest, bytes);
    REG16(NAOMI_DMA_OFFSETH) = (uint16_t)(source_offset >> 16);
    REG16(NAOMI_DMA_OFFSETL) = (uint16_t)source_offset;
    REG32(SB_GDSTAR) = physical;
    REG32(SB_GDLEN) = bytes;
    REG32(SB_GDDIR) = 1u;
    REG32(SB_GDEN) = 1u;
    REG32(SB_GDST) = 1u;

    gd.chunk_sectors = sectors;
    gd.chunk_bytes = bytes;
    gd.started = 1u;
    gd.dma_status = 1u;
    gd.status = VF3_GDC_BUSY;
    return 0;
}

static void poll_cart_dma(void)
{
    if (gd.status != VF3_GDC_BUSY || !gd.started)
        return;
    gd.dma_status = REG32(SB_GDST);
    if ((gd.dma_status & 1u) == 0u) {
        REG32(SB_GDEN) = 0u;
        gd.transferred_bytes += gd.chunk_bytes;
        gd.current_fad += gd.chunk_sectors;
        gd.current_dest += gd.chunk_bytes;
        gd.remaining_sectors -= gd.chunk_sectors;
        gd.started = 0u;
        gd.chunk_sectors = 0u;
        gd.chunk_bytes = 0u;
        if (gd.remaining_sectors == 0u) {
            gd.status = VF3_GDC_COMPLETE;
        } else if (start_cart_dma_chunk() != 0) {
            gd.status = (uint32_t)VF3_GDC_ERR;
        }
    }
}

static uint32_t remaining_sectors(void)
{
    return gd.remaining_sectors;
}

static uint32_t vf3_gd_syscall_handler(uint32_t r4, uint32_t r5,
                                       uint32_t r6, uint32_t r7)
{
    volatile uint32_t *words;
    if (r6 != 0u) {
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
        if (gd.command == VF3_GD_CMD_DMAREAD || gd.command == VF3_GD_CMD_PIOREAD) {
            uint64_t bytes = (uint64_t)gd.params[1] * VF3_GD_SECTOR_BYTES;
            if (gd.params[1] == 0u || bytes > MAX_REQUEST_BYTES ||
                gd.params[0] > 0xffffffffu - gd.params[1]) {
                gd.status = (uint32_t)VF3_GDC_ERR;
            } else {
                gd.total_bytes = (uint32_t)bytes;
                gd.current_fad = gd.params[0];
                gd.remaining_sectors = gd.params[1];
                gd.current_dest = gd.params[2];
            }
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
                if (start_cart_dma_chunk() != 0)
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
        gd.remaining_sectors = 0u;
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
