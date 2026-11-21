#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>
#include <dc/minifont.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "motion_aim.h"
#include "aim_math.h"
#include "matrix.h"
#include "pose_orchestration.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define RAM_BASE 0x0C010000u
#define RAM_SIZE 0x200000u

static float ram[RAM_SIZE / sizeof(float)];
static char lines[8][64];
static pvr_ptr_t font_texture;
static int pass_count, fail_count;

static void check(int ok, const char *name)
{
    snprintf(lines[pass_count + fail_count], sizeof lines[0], "%s: %s",
             name, ok ? "PASS" : "FAIL");
    if (ok) ++pass_count; else ++fail_count;
}

static int nearf(float a, float b)
{
    return isfinite(a) && fabsf(a - b) < 0.0005f;
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
    int ix = (c % 16) * 16, iy = (c / 16) * 24;
    float u1 = ix / 256.0f, v1 = iy / 256.0f;
    float u2 = (ix + 12) / 256.0f, v2 = (iy + 24) / 256.0f;
    if (c <= ' ' || c >= 127) return;
    memset(&v, 0, sizeof v);
    v.flags = PVR_CMD_VERTEX; v.z = 1.0f; v.u = u1; v.v = v2;
    v.x = x; v.y = y + 24; v.argb = PVR_PACK_COLOR(1,1,1,1);
    pvr_prim(&v, sizeof v);
    v.x = x; v.y = y; v.v = v1; pvr_prim(&v, sizeof v);
    v.x = x + 12; v.y = y + 24; v.u = u2; v.v = v2;
    pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.x = x + 12; v.y = y; v.v = v1;
    pvr_prim(&v, sizeof v);
}

static void draw_text(float x, float y, const char *s)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t poly;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
        PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
        256, 256, font_texture, PVR_FILTER_NONE);
    pvr_poly_compile(&poly, &cxt);
    pvr_prim(&poly, sizeof poly);
    for (size_t i = 0; s[i]; ++i) {
        draw_char(x, y, (unsigned char)s[i]);
        x += 12;
    }
}

int main(void)
{
    float identity[16], rotated[16], result[16];
    const float point[3] = { 2.0f, 3.0f, 4.0f };
    float transformed[3];
    vf3_native_aim aim;
    memset(&aim, 0, sizeof aim);
    aim.ram = ram;
    aim.ram_base = RAM_BASE;
    aim.f = 0x0C200000u;
    vf3_native_matrix_identity(identity);
    check(nearf(identity[0],1) && nearf(identity[5],1) &&
          nearf(identity[10],1) && nearf(identity[15],1) &&
          nearf(identity[1],0) && nearf(identity[14],0),
          "MATRIX IDENTITY");

    vf3_native_matrix_identity(rotated);
    rotated[0] = 0.0f; rotated[1] = 1.0f;
    rotated[4] = -1.0f; rotated[5] = 0.0f;
    vf3_native_matrix_multiply(identity, rotated, result);
    check(nearf(result[0],0) && nearf(result[1],1) &&
          nearf(result[4],-1) && nearf(result[5],0),
          "MATRIX MULTIPLY");

    vf3_native_matrix_transform_point(rotated, point, transformed);
    check(nearf(transformed[0],-3) && nearf(transformed[1],2) &&
          nearf(transformed[2],4),
          "POINT TRANSFORM");

    memcpy(aim.xm, rotated, sizeof rotated);
    vf3_aim_push(&aim, 0);
    vf3_aim_identity(&aim);
    vf3_aim_pop(&aim);
    check(aim.sp == 0 && nearf(aim.xm[0],0) && nearf(aim.xm[1],1) &&
          nearf(aim.xm[4],-1) && nearf(aim.xm[5],0),
          "POSE MATRIX STACK");

    vf3_native_matrix_multiply(rotated, rotated, result);
    check(nearf(result[0],-1) && nearf(result[5],-1) &&
          nearf(result[10],1) && nearf(result[15],1),
          "COMPOSED ROTATION");

    check(sizeof(vf3_native_aim) >= sizeof(float) * (16*16 + 16 + 16),
          "POSE CONTEXT LAYOUT");

    pvr_init_defaults();
    pvr_set_bg_color(0.015f, 0.025f, 0.055f);
    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if (!font_texture) for (;;) thd_sleep(1000);
    font_init();
    for (;;) {
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_TR_POLY);
        draw_text(16, 12, "VF3 NAOMI - NATIVE POSE CORE");
        for (int i = 0; i < pass_count + fail_count; ++i)
            draw_text(16, 60 + i * 32, lines[i]);
        char summary[64];
        snprintf(summary, sizeof summary, "RESULT: %d PASS / %d FAIL",
                 pass_count, fail_count);
        draw_text(16, 420, summary);
        pvr_list_finish();
        pvr_scene_finish();
        thd_sleep(16);
    }
    return 0;
}
