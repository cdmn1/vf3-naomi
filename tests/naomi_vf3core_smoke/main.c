/*
 * VF3 Naomi code smoke test.
 *
 * Runs the real MT motion-table parser from celsowm/vf3-decomp on Naomi.
 * The fixture is synthetic and contains no commercial game data.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>
#include "mt.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define FONT_W 12
#define FONT_H 24

static pvr_ptr_t font_texture;
static int parser_ok;
static unsigned parsed_count, record_count;
static uint32_t record_slot, record_offset, record_length;

static void record_cb(uint32_t slot, uint32_t off, uint32_t len, void *ctx) {
    (void)ctx;
    ++record_count;
    record_slot = slot;
    record_offset = off;
    record_length = len;
}

static void run_vf3_parser(void) {
    const uint32_t size = MT_PAYLOAD_FLOOR + 4;
    uint8_t *fixture = (uint8_t *)calloc(size, 1);
    MtFile file;
    MtFile invalid;

    if (!fixture)
        return;

    /* One valid slot points at a four-byte record at the payload floor. */
    fixture[0] = (uint8_t)(MT_PAYLOAD_FLOOR & 0xff);
    fixture[1] = (uint8_t)((MT_PAYLOAD_FLOOR >> 8) & 0xff);
    fixture[2] = (uint8_t)((MT_PAYLOAD_FLOOR >> 16) & 0xff);
    fixture[3] = (uint8_t)((MT_PAYLOAD_FLOOR >> 24) & 0xff);

    if (mt_parse(fixture, size, &file) != 0) {
        free(fixture);
        return;
    }

    parsed_count = file.count;
    mt_each(&file, record_cb, NULL);

    /* Also exercise the parser's short-input rejection path. */
    parser_ok = file.count == 1 &&
                file.first_record_off == MT_PAYLOAD_FLOOR &&
                record_count == 1 &&
                record_slot == 0 &&
                record_offset == MT_PAYLOAD_FLOOR &&
                record_length == 4 &&
                mt_parse(fixture, MT_PAYLOAD_FLOOR, &invalid) == -1;

    free(fixture);
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
    const float u1 = ix / 256.0f;
    const float v1 = iy / 256.0f;
    const float u2 = (ix + FONT_W) / 256.0f;
    const float v2 = (iy + FONT_H) / 256.0f;
    if (c <= ' ' || c >= 127) return;

    v.flags = PVR_CMD_VERTEX;
    v.x = x; v.y = y + FONT_H; v.z = 1.0f;
    v.u = u1; v.v = v2;
    v.argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
    v.oargb = 0;
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
    char line[96];
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(48.0f, 56.0f, "VIRTUA FIGHTER 3 - NAOMI");
    draw_text(48.0f, 96.0f, "REAL VF3 C CODE EXECUTED");
    draw_text(48.0f, 144.0f, parser_ok ? "MT MOTION PARSER: PASS" : "MT MOTION PARSER: FAIL");
    snprintf(line, sizeof(line), "SLOTS %u  RECORDS %u", parsed_count, record_count);
    draw_text(48.0f, 184.0f, line);
    snprintf(line, sizeof(line), "RECORD OFFSET %06X  SIZE %u", (unsigned)record_offset, (unsigned)record_length);
    draw_text(48.0f, 224.0f, line);
    draw_text(48.0f, 280.0f, "SYNTHETIC FIXTURE - NO GAME DATA");
    pvr_list_finish();
    pvr_scene_finish();
}

int main(void) {
    run_vf3_parser();
    pvr_init_defaults();
    pvr_set_bg_color(0.02f, 0.06f, 0.12f);
    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if (!font_texture) {
        printf("VF3 MT parser smoke: font texture allocation failed\n");
        for (;;) thd_sleep(1000);
    }
    font_init();
    for (;;) {
        draw_screen();
        thd_sleep(16);
    }
    return parser_ok ? 0 : 1;
}
