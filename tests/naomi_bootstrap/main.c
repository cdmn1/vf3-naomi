/*
 * VF3 Naomi bootstrap.
 *
 * This is deliberately independent of MIE/JVS, disc access and game assets.
 * Its only job is to prove that our Naomi KOS executable boots and renders.
 */
#include <stdint.h>
#include <stdio.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

#define FONT_W 12
#define FONT_H 24

static pvr_ptr_t font_texture;

static void font_init(void) {
    uint16_t *vram = (uint16_t *)font_texture;
    for(int y = 0; y < 8; ++y) {
        for(int x = 0; x < 16; ++x) {
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

    if(c <= ' ' || c >= 127) return;

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
    if(!s || !s[0]) return;

    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
                     PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     256, 256, font_texture, PVR_FILTER_NONE);
    pvr_poly_compile(&poly, &cxt);
    pvr_prim(&poly, sizeof(poly));

    for(size_t i = 0; s[i]; ++i) {
        draw_char(x, y, (unsigned char)s[i]);
        x += FONT_W;
    }
}

static void draw_screen(void) {
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(48.0f, 72.0f, "VIRTUA FIGHTER 3 - NAOMI");
    draw_text(48.0f, 120.0f, "BOOTSTRAP EXECUTABLE RUNNING");
    draw_text(48.0f, 168.0f, "KALLISTIOS / POWERVR2 OK");
    draw_text(48.0f, 240.0f, "GAME RUNTIME NOT LINKED YET");
    pvr_list_finish();
    pvr_scene_finish();
}

int main(void) {
    pvr_init_defaults();
    pvr_set_bg_color(0.02f, 0.06f, 0.12f);

    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if(!font_texture) {
        printf("VF3 Naomi bootstrap: font texture allocation failed\n");
        for(;;) thd_sleep(1000);
    }
    font_init();

    for(;;) {
        draw_screen();
        thd_sleep(16);
    }
    return 0;
}
