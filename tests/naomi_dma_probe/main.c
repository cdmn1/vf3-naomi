/*
 * VF3 Naomi cartridge DMA probe.
 *
 * Compares a short read through Naomi cartridge PIO with a System Bus GD-DMA
 * transfer from the same cartridge/NetDIMM image offset. No Dreamcast BIOS
 * GD-ROM syscalls or commercial assets are used.
 */
#include <stdint.h>
#include <stdio.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

#define NAOMI_ROM_OFFSETH  0x005f7000u
#define NAOMI_ROM_OFFSETL  0x005f7004u
#define NAOMI_ROM_DATA     0x005f7008u
#define NAOMI_DMA_OFFSETH  0x005f700cu
#define NAOMI_DMA_OFFSETL  0x005f7010u

#define SB_GDSTAR          0x005f7404u
#define SB_GDLEN           0x005f7408u
#define SB_GDDIR           0x005f740cu
#define SB_GDEN            0x005f7414u
#define SB_GDST            0x005f7418u

#define WORDS 16
#define DMA_BYTES (WORDS * 2)

static uint16_t pio_words[WORDS];
static uint8_t dma_buffer[DMA_BYTES] __attribute__((aligned(32)));
static pvr_ptr_t font_texture;
static int pio_ok, dma_ok, compare_ok;
static uint32_t dma_status, dma_loops;

static void read_pio(void) {
    REG16(NAOMI_ROM_OFFSETH) = 0x8000; /* auto-increment */
    REG16(NAOMI_ROM_OFFSETL) = 0;
    for (unsigned i = 0; i < WORDS; ++i)
        pio_words[i] = REG16(NAOMI_ROM_DATA);

    pio_ok = 0;
    for (unsigned i = 0; i < WORDS; ++i)
        if (pio_words[i] != 0 && pio_words[i] != 0xffff)
            pio_ok = 1;
}

static void read_dma(void) {
    REG16(NAOMI_DMA_OFFSETH) = 0;
    REG16(NAOMI_DMA_OFFSETL) = 0;

    for (unsigned i = 0; i < DMA_BYTES; ++i)
        dma_buffer[i] = 0;

    /* SB_GDSTAR is a root-bus/physical RAM address. */
    REG32(SB_GDSTAR) = (uint32_t)((uintptr_t)dma_buffer & 0x1fffffffu);
    REG32(SB_GDLEN) = DMA_BYTES;
    REG32(SB_GDDIR) = 1; /* cartridge -> system RAM */
    REG32(SB_GDEN) = 1;
    REG32(SB_GDST) = 1;

    dma_loops = 0;
    while ((REG32(SB_GDST) & 1u) && dma_loops < 3000) {
        ++dma_loops;
        thd_sleep(1);
    }
    dma_status = REG32(SB_GDST);
    REG32(SB_GDEN) = 0;
    dma_ok = !(dma_status & 1u) && dma_loops < 3000;
}

static void compare_reads(void) {
    /* Read the DMA destination through the uncached P2 alias. */
    const volatile uint16_t *dma_words =
        (const volatile uint16_t *)((uintptr_t)dma_buffer | 0x20000000u);
    compare_ok = pio_ok && dma_ok;
    for (unsigned i = 0; i < WORDS && compare_ok; ++i)
        if (pio_words[i] != dma_words[i])
            compare_ok = 0;
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
    const int ix = (c % 16) * 16, iy = (c / 16) * 24;
    const float u1 = ix / 256.0f, v1 = iy / 256.0f;
    const float u2 = (ix + 12) / 256.0f, v2 = (iy + 24) / 256.0f;
    if (c <= ' ' || c >= 127) return;
    v.flags = PVR_CMD_VERTEX; v.x = x; v.y = y + 24; v.z = 1;
    v.u = u1; v.v = v2; v.argb = PVR_PACK_COLOR(1,1,1,1); v.oargb = 0;
    pvr_prim(&v, sizeof(v));
    v.x = x; v.y = y; v.u = u1; v.v = v1; pvr_prim(&v, sizeof(v));
    v.x = x + 12; v.y = y + 24; v.u = u2; v.v = v2; pvr_prim(&v, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x + 12; v.y = y; v.u = u2; v.v = v1; pvr_prim(&v, sizeof(v));
}

static void draw_text(float x, float y, const char *s) {
    pvr_poly_cxt_t cxt; pvr_poly_hdr_t poly;
    if (!s || !s[0]) return;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
        PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
        256, 256, font_texture, PVR_FILTER_NONE);
    pvr_poly_compile(&poly, &cxt); pvr_prim(&poly, sizeof(poly));
    for (size_t i = 0; s[i]; ++i) { draw_char(x, y, (unsigned char)s[i]); x += 12; }
}

static void draw_screen(void) {
    char line[80];
    pvr_wait_ready(); pvr_scene_begin(); pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(40, 48, "VF3 NAOMI CART DMA PROBE");
    draw_text(40, 88, pio_ok ? "CART PIO READ: PASS" : "CART PIO READ: FAIL");
    draw_text(40, 128, dma_ok ? "SYSTEM BUS GD DMA: PASS" : "SYSTEM BUS GD DMA: FAIL");
    draw_text(40, 168, compare_ok ? "PIO / DMA DATA MATCH: PASS" : "PIO / DMA DATA MATCH: FAIL");
    snprintf(line, sizeof(line), "DMA STATUS %08X  WAIT %u ms", (unsigned)dma_status, (unsigned)dma_loops);
    draw_text(40, 208, line);
    draw_text(40, 264, "READS IMAGE OFFSET ZERO - NO BIOS SYSCALL");
    pvr_list_finish(); pvr_scene_finish();
}

int main(void) {
    read_pio();
    read_dma();
    compare_reads();

    pvr_init_defaults();
    pvr_set_bg_color(0.02f, 0.06f, 0.12f);
    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if (!font_texture) for (;;) thd_sleep(1000);
    font_init();
    for (;;) { draw_screen(); thd_sleep(16); }
    return compare_ok ? 0 : 1;
}
