/*
 * VF3 Naomi MIE/JVS input probe.
 *
 * Build with a KallistiOS toolchain configured for Naomi. This is a
 * standalone hardware probe; it does not boot or modify VF3.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/arch.h>

#include "vf3_input_map.h"

#include <kos.h>
#include <kos/init.h>
#include <dc/minifont.h>
#include <dc/maple.h>
#include <dc/maple/mie.h>
#include <dc/pvr.h>

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MIE);

#define UI_LINE_MAX 64
#define UI_LINE_COUNT 12
#define UI_FONT_W 12
#define UI_FONT_H 24

static pvr_ptr_t ui_font_tex;

static const char *port_mode_name(mie_port0_mode_t mode)
{
    switch(mode) {
    case MIE_PORT0_JVS:   return "MIE/JVS";
    case MIE_PORT0_MAPLE: return "standard Maple";
    default:              return "unknown / scan pending";
    }
}

static void ui_font_init(void)
{
    uint16_t *vram = (uint16_t *)ui_font_tex;

    for(int y = 0; y < 8; ++y) {
        for(int x = 0; x < 16; ++x) {
            minifont_draw(vram, 256, (uint32_t)(y * 16 + x));
            vram += 16;
        }
        vram += 23 * 256;
    }
}

static void ui_draw_char(float x, float y, int c)
{
    pvr_vertex_t v;
    const int ix = (c % 16) * 16;
    const int iy = (c / 16) * 24;
    const float u1 = ix / 256.0f;
    const float v1 = iy / 256.0f;
    const float u2 = (ix + UI_FONT_W) / 256.0f;
    const float v2 = (iy + UI_FONT_H) / 256.0f;

    if(c <= ' ' || c >= 127) {
        return;
    }

    v.flags = PVR_CMD_VERTEX;
    v.x = x; v.y = y + UI_FONT_H; v.z = 1.0f;
    v.u = u1; v.v = v2;
    v.argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
    v.oargb = 0;
    pvr_prim(&v, sizeof(v));

    v.x = x; v.y = y; v.u = u1; v.v = v1;
    pvr_prim(&v, sizeof(v));

    v.x = x + UI_FONT_W; v.y = y + UI_FONT_H; v.u = u2; v.v = v2;
    pvr_prim(&v, sizeof(v));

    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x + UI_FONT_W; v.y = y; v.u = u2; v.v = v1;
    pvr_prim(&v, sizeof(v));
}

static void ui_draw_str(float x, float y, const char *str)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t poly;

    if(!str || !str[0]) {
        return;
    }

    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
                     PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     256, 256, ui_font_tex, PVR_FILTER_NONE);
    pvr_poly_compile(&poly, &cxt);
    pvr_prim(&poly, sizeof(poly));

    for(size_t i = 0; str[i]; ++i) {
        ui_draw_char(x, y, (unsigned char)str[i]);
        x += UI_FONT_W;
    }
}

static void ui_draw(const char lines[][UI_LINE_MAX], int count)
{
    float y = 8.0f;

    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);

    for(int i = 0; i < count; ++i) {
        ui_draw_str(8.0f, y, lines[i]);
        y += UI_FONT_H;
    }

    pvr_list_finish();
    pvr_scene_finish();
}

static int build_report(char lines[][UI_LINE_MAX])
{
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_MIE);
    mie_state_t *state = dev ? (mie_state_t *)maple_dev_status(dev) : NULL;

    snprintf(lines[0], UI_LINE_MAX, "VF3 NAOMI MIE/JVS INPUT PROBE");
    snprintf(lines[1], UI_LINE_MAX, "Port A: %s", port_mode_name(mie_port0_mode()));

    if(!state) {
        snprintf(lines[2], UI_LINE_MAX, "MIE device: not enumerated");
        snprintf(lines[3], UI_LINE_MAX, "Waiting for MIE/JVS input...");
        for(int i = 4; i < UI_LINE_COUNT; ++i) lines[i][0] = '\0';
        snprintf(lines[11], UI_LINE_MAX, "TEST: RETURN TO NAOMI SYSTEM MENU");
        return -1;
    }

    snprintf(lines[2], UI_LINE_MAX, "MIE device: detected");
    snprintf(lines[3], UI_LINE_MAX, "P1 JVS: %04x  P2 JVS: %04x",
             state->jvs.p1.raw, state->jvs.p2.raw);
    snprintf(lines[4], UI_LINE_MAX, "SYS: %02x  PANEL: %02x",
             state->jvs.system.raw, state->jvs.panel.psw.raw);
    snprintf(lines[5], UI_LINE_MAX, "KOS default pad bits: %08" PRIx32,
             state->cont.buttons);
    snprintf(lines[6], UI_LINE_MAX, "Coins: %u / %u  pulse:%02x fault:%02x",
             state->jvs.coin[0].count, state->jvs.coin[1].count,
             state->jvs.coin_pulse, state->jvs.coin_fault);
    snprintf(lines[7], UI_LINE_MAX, "Analog 0-3: %u %u %u %u",
             state->jvs.analog[0], state->jvs.analog[1],
             state->jvs.analog[2], state->jvs.analog[3]);
    snprintf(lines[8], UI_LINE_MAX, "Analog 4-7: %u %u %u %u",
             state->jvs.analog[4], state->jvs.analog[5],
             state->jvs.analog[6], state->jvs.analog[7]);
    snprintf(lines[9], UI_LINE_MAX, "P1 Maple (active-low): %04x",
             vf3_jvs_to_maple_buttons(state->jvs.p1.raw));
    snprintf(lines[10], UI_LINE_MAX, "P2 Maple (active-low): %04x",
             vf3_jvs_to_maple_buttons(state->jvs.p2.raw));
    snprintf(lines[11], UI_LINE_MAX, "TEST: RETURN TO NAOMI SYSTEM MENU");
    return !!state->jvs.system.test;
}

int main(int argc, char *argv[])
{
    char lines[UI_LINE_COUNT][UI_LINE_MAX];

    (void)argc;
    (void)argv;

    pvr_init_defaults();
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    ui_font_tex = pvr_mem_malloc(256 * 256 * 2);
    if(!ui_font_tex) {
        printf("VF3 MIE probe: could not allocate font texture\n");
        for(;;) {
            thd_sleep(1000);
        }
    }
    ui_font_init();

    maple_wait_scan();

    /* Ignore TEST if it was already held while the application started. */
    {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_MIE);
        mie_state_t *state = dev ? (mie_state_t *)maple_dev_status(dev) : NULL;
        int prev_test = state ? !!state->jvs.system.test : 0;

        for(;;) {
            int test_pressed = build_report(lines);
            int test_now = test_pressed > 0;

            if(test_now && !prev_test) {
                /*
                 * ARCH_EXIT_MENU performs the complete KOS shutdown first.
                 * The Naomi build patches arch_menu() to call BIOS vector
                 * 0x11 only after that shutdown has completed.
                 */
                arch_set_exit_path(ARCH_EXIT_MENU);
                exit(0);
            }

            prev_test = test_now;
            ui_draw(lines, UI_LINE_COUNT);
            thd_sleep(250);
        }
    }

    return 0;
}
