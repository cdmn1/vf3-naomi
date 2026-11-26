#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>
#include <stdint.h>
#include <stdio.h>
#include "gdrom_syscall.h"
#include "vf3_asset_map.h"

#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define NAOMI_ROM_OFFSETH 0x005f7000u
#define NAOMI_ROM_OFFSETL 0x005f7004u
#define NAOMI_ROM_DATA 0x005f7008u
#define FONT_W 12
#define FONT_H 24
#define WORDS 1024u
#define TEST_FAD VF3_ASSET_TEST_FAD
#define TEST_OFFSET VF3_ASSET_TEST_OFFSET
#if VF3_ASSET_MAP_COUNT > 0
/* 1ST_READ.BIN is 576 sectors; the 1 MiB DMA cap forces two chunks. */
#define DMA_TEST_SECTORS 576u
#else
#define DMA_TEST_SECTORS 1u
#endif

KOS_INIT_FLAGS(INIT_DEFAULT);

static uint16_t pio_words[WORDS];
static uint8_t dma_buffer[DMA_TEST_SECTORS * VF3_GD_SECTOR_BYTES] __attribute__((aligned(32)));
static uint8_t pio_bridge_buffer[WORDS * 2u] __attribute__((aligned(32)));
static pvr_ptr_t font_texture;
static int dma_pass, pio_bridge_pass, direct_match, bridge_match, status_pass;
static uint32_t old_service, dma_status, dma_bytes, dma_request, pio_request;

static void read_cart_pio(void)
{
    /* Naomi cartridge PIO offsets are byte addresses; each data read
       advances the internal address by two bytes when auto-increment is set. */
    REG16(NAOMI_ROM_OFFSETH) = (uint16_t)(0x8000u | ((TEST_OFFSET >> 16) & 0x7fffu));
    REG16(NAOMI_ROM_OFFSETL) = (uint16_t)TEST_OFFSET;
    for (unsigned i = 0; i < WORDS; ++i)
        pio_words[i] = REG16(NAOMI_ROM_DATA);
}

static int compare_uncached(const uint16_t *expected, const uint8_t *buffer)
{
    const volatile uint16_t *actual =
        (const volatile uint16_t *)((uintptr_t)buffer | 0x20000000u);
    for (unsigned i = 0; i < WORDS; ++i)
        if (expected[i] != actual[i])
            return 0;
    return 1;
}

static void run_dma_syscall(void)
{
    uint32_t params[4] = { TEST_FAD, DMA_TEST_SECTORS, (uint32_t)(uintptr_t)dma_buffer, 0u };
    uint32_t remaining = 0xffffffffu, ext[4] = {0,0,0,0};
    uint32_t check = 0u;
    unsigned wait = 0;
    dma_request = vf3_gd_req_cmd(VF3_GD_CMD_DMAREAD, params);
    if (!dma_request) return;
    vf3_gd_req_dma_trans(dma_request, &remaining);
    vf3_gd_exec_server();
    do {
        check = vf3_gd_check_dma_trans(dma_request, &remaining);
        if (check != 0u) break;
        thd_sleep(1);
    } while (++wait < 3000u);
    dma_status = vf3_gd_get_cmd_stat(dma_request, ext);
    dma_bytes = ext[2];
    dma_pass = check == 1u && dma_status == VF3_GDC_COMPLETE &&
               remaining == 0u && dma_bytes == DMA_TEST_SECTORS * VF3_GD_SECTOR_BYTES;
    status_pass = dma_status == VF3_GDC_COMPLETE;
}

static void run_pio_to_dma_bridge(void)
{
    uint32_t p2_dest = 0xa0000000u |
        ((uint32_t)(uintptr_t)pio_bridge_buffer & 0x1fffffffu);
    uint32_t params[4] = { TEST_FAD, 1u, p2_dest, 0u };
    uint32_t remaining = 0xffffffffu, ext[4] = {0,0,0,0};
    uint32_t check = 0u, status;
    unsigned wait = 0;
    pio_request = vf3_gd_req_cmd(VF3_GD_CMD_PIOREAD, params);
    if (!pio_request) return;
    vf3_gd_req_pio_trans(pio_request, &remaining);
    vf3_gd_exec_server();
    do {
        check = vf3_gd_check_pio_trans(pio_request, &remaining);
        if (check != 0u) break;
        thd_sleep(1);
    } while (++wait < 3000u);
    status = vf3_gd_get_cmd_stat(pio_request, ext);
    pio_bridge_pass = check == 1u && status == VF3_GDC_COMPLETE &&
                      remaining == 0u && ext[2] == VF3_GD_SECTOR_BYTES;
}

static void font_init(void)
{
    uint16_t *vram = (uint16_t *)font_texture;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 16; ++x) {
            minifont_draw(vram, 256, (uint32_t)(y * 16 + x));
            vram += 16;
        }
        vram += 23 * 256;
    }
}

static void draw_char(float x, float y, unsigned char c)
{
    pvr_vertex_t v;
    const int ix = (c % 16) * 16, iy = (c / 16) * 24;
    const float u1 = ix / 256.0f, v1 = iy / 256.0f;
    const float u2 = (ix + FONT_W) / 256.0f, v2 = (iy + FONT_H) / 256.0f;
    if (c <= ' ' || c >= 127) return;
    v.flags=PVR_CMD_VERTEX; v.x=x; v.y=y+FONT_H; v.z=1.0f;
    v.u=u1; v.v=v2; v.argb=PVR_PACK_COLOR(1,1,1,1); v.oargb=0;
    pvr_prim(&v,sizeof v);
    v.x=x; v.y=y; v.u=u1; v.v=v1; pvr_prim(&v,sizeof v);
    v.x=x+FONT_W; v.y=y+FONT_H; v.u=u2; v.v=v2; pvr_prim(&v,sizeof v);
    v.flags=PVR_CMD_VERTEX_EOL; v.x=x+FONT_W; v.y=y; v.u=u2; v.v=v1;
    pvr_prim(&v,sizeof v);
}

static void draw_text(float x, float y, const char *s)
{
    pvr_poly_cxt_t cxt; pvr_poly_hdr_t poly;
    if (!s || !s[0]) return;
    pvr_poly_cxt_txr(&cxt,PVR_LIST_TR_POLY,
        PVR_TXRFMT_ARGB1555|PVR_TXRFMT_NONTWIDDLED,
        256,256,font_texture,PVR_FILTER_NONE);
    pvr_poly_compile(&poly,&cxt); pvr_prim(&poly,sizeof poly);
    for (size_t i=0;s[i];++i) { draw_char(x,y,(unsigned char)s[i]); x+=FONT_W; }
}

static void draw_screen(void)
{
    char line[96];
    char resident_line[96];
    pvr_wait_ready(); pvr_scene_begin(); pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(32,32,"VF3 NAOMI GD-ROM SYSCALL BRIDGE");
    draw_text(32,76,"BIOS SERVICE VECTOR: INSTALLED");
    draw_text(32,112,dma_pass?"DMAREAD / HOLLY DMA: PASS":"DMAREAD / HOLLY DMA: FAIL");
    draw_text(32,148,pio_bridge_pass?"PIOREAD TRANSLATED TO DMA: PASS":"PIOREAD TRANSLATED TO DMA: FAIL");
    draw_text(32,184,direct_match?"DIRECT PIO / DMA DATA MATCH: PASS":"DIRECT PIO / DMA DATA MATCH: FAIL");
    draw_text(32,220,bridge_match?"PIOREAD / DMA DATA MATCH: PASS":"PIOREAD / DMA DATA MATCH: FAIL");
    draw_text(32,256,status_pass?"GET_CMD_STAT COMPLETION: PASS":"GET_CMD_STAT COMPLETION: FAIL");
    snprintf(line,sizeof line,"DMA STATUS %08X  BYTES %u",(unsigned)dma_status,(unsigned)dma_bytes);
    draw_text(32,292,line);
    snprintf(line,sizeof line,"OLD SERVICE VECTOR %08X",(unsigned)old_service);
    draw_text(32,328,line);
    snprintf(line,sizeof line,"TEST FAD %u  CART OFFSET %08X",(unsigned)TEST_FAD,(unsigned)TEST_OFFSET);
    draw_text(32,364,line);
    draw_text(32,400,"NO SERIAL CONNECTION REQUIRED");
    snprintf(resident_line, sizeof resident_line, "RESIDENT 8D000000-8D030000");
    draw_text(32,436,resident_line);
    pvr_list_finish(); pvr_scene_finish();
}

int main(void)
{
    read_cart_pio();
    old_service = vf3_gd_install();
    run_dma_syscall();
    /* Preserve the multi-chunk DMA result before the PIOREAD request replaces
       the bridge state with its own one-sector transfer. */
    dma_status = vf3_gd_last_dma_status();
    dma_bytes = vf3_gd_last_bytes();
    direct_match = compare_uncached(pio_words,dma_buffer);
    run_pio_to_dma_bridge();
    bridge_match = compare_uncached(pio_words,pio_bridge_buffer);

    pvr_init_defaults();
    pvr_set_bg_color(0.02f,0.06f,0.12f);
    font_texture = pvr_mem_malloc(256u*256u*2u);
    if (!font_texture) for (;;) thd_sleep(1000);
    font_init();
    for (;;) { draw_screen(); thd_sleep(16); }
    return 0;
}
