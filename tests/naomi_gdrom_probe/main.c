/*
 * Naomi GD-ROM service probe for the VF3 NetDIMM asset-provider path.
 *
 * Reads one sector at the known MTJACLAU.BIN FAD using the BIOS GD-ROM
 * DMAREAD syscall. The sector address comes from GDFS_ASSET_INDEX.csv:
 * GD LBA 475142 + 150-sector FAD offset = 475292.
 *
 * No commercial asset is embedded in this ROM.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>
#include <dc/syscalls.h>
#include <dc/memory.h>
#include <dc/g1ata.h>
#include <kos/cache.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

#define FONT_W 12
#define FONT_H 24
#define VF3_MTJACLAU_FAD 475292u
#define SECTOR_SIZE 2048u

static pvr_ptr_t font_texture;
static int drive_rc = -99;
static int toc_rc = -99;
static int read_rc = -99;
static int drive_status = -1;
static int disc_type = -1;
static uint32_t gdrom_service_ptr;
static uint32_t data_track_fad;
static uint8_t *sector_buf;
static cd_toc_t toc;

static int exec_bios_command(cd_cmd_code_t command, void *params,
                             cd_cmd_chk_status_t *final_status) {
    cd_cmd_chk_status_t status;
    gdc_cmd_hnd_t handle = 0;
    for (int retry = 0; retry < 500; ++retry) {
        handle = syscall_gdrom_send_command(command, params);
        if (handle > 0) break;
        syscall_gdrom_exec_server();
        thd_sleep(1);
    }
    if (handle <= 0) return -20;
    for (int retry = 0; retry < 5000; ++retry) {
        syscall_gdrom_exec_server();
        const cd_cmd_chk_t result = syscall_gdrom_check_command(handle, &status);
        if (result == CD_CMD_COMPLETED) {
            if (final_status) *final_status = status;
            return 0;
        }
        if (result == CD_CMD_FAILED || result == CD_CMD_NOT_FOUND) {
            if (final_status) *final_status = status;
            return -21 - (int)status.err1;
        }
        thd_sleep(1);
    }
    syscall_gdrom_abort_command(handle);
    return -22;
}

static uint32_t locate_data_track(const cd_toc_t *table) {
    const int first = (int)((table->first >> 16) & 0xffu);
    const int last = (int)((table->last >> 16) & 0xffu);
    for (int track = last; track >= first && track >= 1 && track <= 99; --track) {
        const uint32_t entry = table->entry[track - 1];
        if (((entry >> 28) & 0x0fu) == 4u)
            return entry & 0x00ffffffu;
    }
    return 0;
}

static int read_fad_dma(uint32_t fad, uint8_t *buffer) {
    cd_read_params_t params;
    cd_cmd_chk_status_t status;

    if (!buffer || (((uintptr_t)buffer & 31u) != 0))
        return -10;
    memset(buffer, 0, SECTOR_SIZE);
    dcache_inval_range((uintptr_t)buffer, SECTOR_SIZE);

    params.start_sec = fad;
    params.num_sec = 1;
    params.buffer = (void *)((uintptr_t)buffer & MEM_AREA_CACHE_MASK);
    params.is_test = 0;

    /* Same BIOS DMAREAD command (0x11) used by VF3's original GDFS loader. */
    const int result = exec_bios_command(CD_CMD_DMAREAD, &params, &status);
    if (result == 0)
        dcache_inval_range((uintptr_t)buffer, SECTOR_SIZE);
    return result;
}

static void font_init(void) {
    uint16_t *vram = (uint16_t *)font_texture;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 16; ++x) {
            minifont_draw(vram, 256, (uint32_t)(y * 16 + x));
            vram += 16;
        }
        vram += 23 * 256;
    }
}

static void draw_char(float x, float y, unsigned char c) {
    pvr_vertex_t v;
    const int ix = (c % 16) * 16;
    const int iy = (c / 16) * 24;
    const float u1 = ix / 256.0f, v1 = iy / 256.0f;
    const float u2 = (ix + FONT_W) / 256.0f;
    const float v2 = (iy + FONT_H) / 256.0f;
    if (c <= ' ' || c >= 127) return;

    v.flags = PVR_CMD_VERTEX;
    v.x = x; v.y = y + FONT_H; v.z = 1.0f;
    v.u = u1; v.v = v2;
    v.argb = PVR_PACK_COLOR(1, 1, 1, 1); v.oargb = 0;
    pvr_prim(&v, sizeof(v));
    v.x = x; v.y = y; v.u = u1; v.v = v1;
    pvr_prim(&v, sizeof(v));
    v.x = x + FONT_W; v.y = y + FONT_H; v.u = u2; v.v = v2;
    pvr_prim(&v, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x + FONT_W; v.y = y; v.u = u2; v.v = v1;
    pvr_prim(&v, sizeof(v));
}

static void draw_text(float x, float y, const char *s) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t poly;
    if (!s || !s[0]) return;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
                     PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     256, 256, font_texture, PVR_FILTER_NONE);
    pvr_poly_compile(&poly, &cxt);
    pvr_prim(&poly, sizeof(poly));
    for (size_t i = 0; s[i]; ++i) {
        draw_char(x, y, (unsigned char)s[i]);
        x += FONT_W;
    }
}

static void draw_screen(void) {
    char line[96], bytes[96];
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);

    draw_text(40, 40, "VF3 NAOMI GD-ROM PROBE");
    snprintf(line, sizeof(line), "BIOS GDROM VECTOR: %08X", (unsigned)gdrom_service_ptr);
    draw_text(40, 72, line);
    snprintf(line, sizeof(line), "DRIVE STATUS CALL: %s", drive_rc == 0 ? "PASS" : "FAIL");
    draw_text(40, 104, line);
    snprintf(line, sizeof(line), "STATUS %d  DISC TYPE %02X", drive_status, disc_type & 0xff);
    draw_text(40, 136, line);
    snprintf(line, sizeof(line), "HIGH DENSITY TOC: %s", toc_rc == 0 ? "PASS" : "FAIL");
    draw_text(40, 176, line);
    snprintf(line, sizeof(line), "DATA TRACK FAD: %06X", (unsigned)data_track_fad);
    draw_text(40, 208, line);
    snprintf(line, sizeof(line), "MTJACLAU FAD: %06X", (unsigned)VF3_MTJACLAU_FAD);
    draw_text(40, 248, line);
    snprintf(line, sizeof(line), "BIOS DMAREAD: %s (%d)", read_rc == 0 ? "PASS" : "FAIL", read_rc);
    draw_text(40, 288, line);

    if (read_rc == 0 && sector_buf) {
        snprintf(bytes, sizeof(bytes), "FIRST 8 BYTES: %02X %02X %02X %02X %02X %02X %02X %02X",
                 sector_buf[0], sector_buf[1], sector_buf[2], sector_buf[3],
                 sector_buf[4], sector_buf[5], sector_buf[6], sector_buf[7]);
        draw_text(40, 328, bytes);
    } else {
        draw_text(40, 328, "MOUNT THE VF3TB GD-ROM IMAGE");
    }
    draw_text(40, 384, "READ-ONLY TEST - NO GAME DATA INCLUDED");

    pvr_list_finish();
    pvr_scene_finish();
}

static void unlock_gd_dma(void) {
    volatile uint32_t *protection =
        (volatile uint32_t *)(G1_ATA_DMA_PROTECTION | MEM_AREA_P2_BASE);
    *protection = G1_ATA_DMA_UNLOCK_ALLMEM;
}

static int valid_service_pointer(uint32_t ptr) {
    return (ptr >= 0x8C000000u && ptr < 0x8D000000u) ||
           (ptr >= 0xA0000000u && ptr < 0xA1000000u) ||
           (ptr >= 0x0C000000u && ptr < 0x0D000000u);
}

int main(void) {
    gdrom_service_ptr = *(volatile uint32_t *)0x8C0000BCu;
    if (valid_service_pointer(gdrom_service_ptr)) {
        /* KOS's Dreamcast cdrom_init() performs this DMA-protection unlock. */
        unlock_gd_dma();
        syscall_gdrom_init();

        cd_check_drive_status_t drive_info;
        memset(&drive_info, 0, sizeof(drive_info));
        drive_rc = syscall_gdrom_check_drive(&drive_info);
        if (drive_rc == 0) {
            drive_status = (int)drive_info.status;
            disc_type = (int)drive_info.disc_type;
        }

        cd_cmd_toc_params_t toc_params;
        toc_params.area = CD_AREA_HIGH;
        toc_params.buffer = &toc;
        toc_rc = exec_bios_command(CD_CMD_GETTOC2, &toc_params, NULL);
        if (toc_rc == 0)
            data_track_fad = locate_data_track(&toc);
    } else {
        drive_rc = -30;
        toc_rc = -30;
    }

    sector_buf = (uint8_t *)memalign(32, SECTOR_SIZE);
    if (sector_buf)
        read_rc = read_fad_dma(VF3_MTJACLAU_FAD, sector_buf);

    pvr_init_defaults();
    pvr_set_bg_color(0.02f, 0.06f, 0.12f);
    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if (!font_texture) {
        printf("VF3 GD-ROM probe: font allocation failed\n");
        for (;;) thd_sleep(1000);
    }
    font_init();
    for (;;) {
        draw_screen();
        thd_sleep(16);
    }
    return 0;
}
