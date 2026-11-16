/*
 * Naomi title-test experiment for the VF3 MIE/JVS probe.
 *
 * This is built as a separate KOS executable and placed in the Naomi ROM's
 * test segment/entry point. EXIT uses KOS's BIOS-menu exit path, which still
 * needs confirmation on the target Naomi BIOS.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <kos.h>
#include <kos/init.h>
#include <arch/arch.h>
#include <dc/minifont.h>
#include <dc/maple.h>
#include <dc/maple/mie.h>
#include <dc/pvr.h>

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MIE);

#define TEST_LINE_MAX 64
#define LINE_COUNT 12
#define FONT_W 12
#define FONT_H 24

static pvr_ptr_t font_tex;

static void font_init(void)
{
    uint16_t *vram = (uint16_t *)font_tex;
    for(int y = 0; y < 8; ++y) {
        for(int x = 0; x < 16; ++x) {
            minifont_draw(vram, 256, (uint32_t)(y * 16 + x));
            vram += 16;
        }
        vram += 23 * 256;
    }
}

static void draw_char(float x, float y, int c)
{
    pvr_vertex_t v;
    int ix = (c % 16) * 16;
    int iy = (c / 16) * 24;
    float u1 = ix / 256.0f, v1 = iy / 256.0f;
    float u2 = (ix + FONT_W) / 256.0f;
    float v2 = (iy + FONT_H) / 256.0f;

    if(c <= ' ' || c >= 127) return;
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

static void draw_string(float x, float y, const char *s)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t poly;
    if(!s || !s[0]) return;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
                     PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     256, 256, font_tex, PVR_FILTER_NONE);
    pvr_poly_compile(&poly, &cxt);
    pvr_prim(&poly, sizeof(poly));
    for(size_t i = 0; s[i]; ++i) {
        draw_char(x, y, (unsigned char)s[i]);
        x += FONT_W;
    }
}

static void draw_lines(const char lines[][TEST_LINE_MAX])
{
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    for(int i = 0; i < LINE_COUNT; ++i)
        draw_string(8.0f, 8.0f + i * FONT_H, lines[i]);
    pvr_list_finish();
    pvr_scene_finish();
}

static mie_state_t *mie_status(void)
{
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_MIE);
    return dev ? (mie_state_t *)maple_dev_status(dev) : NULL;
}

static void render_menu(char lines[][TEST_LINE_MAX], int selected,
                        mie_state_t *s)
{
    snprintf(lines[0], TEST_LINE_MAX, "VF3 GAME TEST MODE");
    snprintf(lines[1], TEST_LINE_MAX, "------------------------------");
    snprintf(lines[2], TEST_LINE_MAX, "%s INPUT TEST", selected == 0 ? "->" : "  ");
    snprintf(lines[3], TEST_LINE_MAX, "%s EXIT", selected == 1 ? "->" : "  ");
    lines[4][0] = 0;
    snprintf(lines[5], TEST_LINE_MAX, "SELECT BY SERVICE BUTTON");
    snprintf(lines[6], TEST_LINE_MAX, "AND PRESS TEST BUTTON");
    lines[7][0] = 0;
    if(s) {
        snprintf(lines[8], TEST_LINE_MAX, "MIE: FOUND  PANEL: %02X", s->jvs.panel.psw.raw);
        snprintf(lines[9], TEST_LINE_MAX, "JVS TEST:%u SERVICE:%u",
                 s->jvs.system.test, s->jvs.p1.service);
        snprintf(lines[10], TEST_LINE_MAX, "SYSTEM RAW:%02X P1 RAW:%04X",
                 s->jvs.system.raw, s->jvs.p1.raw);
    } else {
        snprintf(lines[8], TEST_LINE_MAX, "MIE: NOT FOUND");
        snprintf(lines[9], TEST_LINE_MAX, "PANEL INPUT DATA UNAVAILABLE");
        snprintf(lines[10], TEST_LINE_MAX, "CHECK INIT_MIE / MAPLE SCAN");
    }
    snprintf(lines[11], TEST_LINE_MAX, "CONTROLS: JVS SYSTEM TEST / P1 SERVICE");
}

static void render_input(char lines[][TEST_LINE_MAX], mie_state_t *s)
{
    snprintf(lines[0], TEST_LINE_MAX, "VF3 GAME TEST MODE");
    snprintf(lines[1], TEST_LINE_MAX, "INPUT TEST");
    if(!s) {
        snprintf(lines[2], TEST_LINE_MAX, "MIE/JVS DEVICE NOT FOUND");
        for(int i = 3; i < LINE_COUNT; ++i) lines[i][0] = 0;
        snprintf(lines[11], TEST_LINE_MAX, "PRESS TEST TO RETURN");
        return;
    }
    snprintf(lines[2], TEST_LINE_MAX, "P1 JVS SWITCHES: %04X", s->jvs.p1.raw);
    snprintf(lines[3], TEST_LINE_MAX, "P2 JVS SWITCHES: %04X", s->jvs.p2.raw);
    snprintf(lines[4], TEST_LINE_MAX, "SYSTEM: %02X PANEL: %02X",
             s->jvs.system.raw, s->jvs.panel.psw.raw);
    snprintf(lines[5], TEST_LINE_MAX, "JVS TEST: %u SERVICE: %u",
             s->jvs.system.test, s->jvs.p1.service);
    snprintf(lines[6], TEST_LINE_MAX, "P1 START: %u", s->jvs.p1.start);
    snprintf(lines[7], TEST_LINE_MAX, "COINS: %u / %u",
             s->jvs.coin[0].count, s->jvs.coin[1].count);
    snprintf(lines[8], TEST_LINE_MAX, "ANALOG: %u %u %u %u",
             s->jvs.analog[0], s->jvs.analog[1],
             s->jvs.analog[2], s->jvs.analog[3]);
    snprintf(lines[9], TEST_LINE_MAX, "PANEL RAW: %02X SYSTEM RAW: %02X",
             s->jvs.panel.psw.raw, s->jvs.system.raw);
    lines[10][0] = 0;
    snprintf(lines[11], TEST_LINE_MAX, "PRESS TEST TO RETURN TO MENU");
}

int main(int argc, char **argv)
{
    char lines[LINE_COUNT][TEST_LINE_MAX] = {{0}};
    int selected = 0;
    int page = 0;
    int prev_service = 0, prev_test = 0;
    (void)argc; (void)argv;

    pvr_init_defaults();
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    font_tex = pvr_mem_malloc(256 * 256 * 2);
    if(!font_tex) for(;;) thd_sleep(1000);
    font_init();
    maple_wait_scan();

    int exit_requested = 0;
    for(;;) {
        mie_state_t *s = mie_status();
        int service = s ? !!s->jvs.p1.service : 0;
        int test = s ? !!s->jvs.system.test : 0;
        int service_edge = service && !prev_service;
        int test_edge = test && !prev_test;
        prev_service = service;
        prev_test = test;

        if(page == 0) {
            if(service_edge) selected = (selected + 1) % 2;
            if(test_edge) {
                if(selected == 0) {
                    page = 1;
                } else {
                    /*
                     * Leave the title test program through KOS's BIOS-menu
                     * path, not its default return-to-loader path.
                     */
                    arch_set_exit_path(ARCH_EXIT_MENU);
                    exit_requested = 1;
                }
            }
            render_menu(lines, selected, s);
        } else if(page == 1) {
            if(test_edge) page = 0;
            render_input(lines, s);
        }
        draw_lines(lines);
        if(exit_requested) break;
        thd_sleep(50);
    }
    return 0;
}
