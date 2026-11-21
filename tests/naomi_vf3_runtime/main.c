/*
 * VF3 Naomi real MT source-row and 63-channel evaluator probe.
 *
 * Uses the real MTAKIPAI.BIN source rows for motion IDs 1050 and 2386.
 * The upstream MT reader expands the packed 2-bit channel map, bounds the
 * count/time/tuple streams, and the native evaluator samples several Q8
 * phases. This proves source decoding and channel evaluation, not yet the
 * channel-to-bone/POL mapping or visible character playback.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>
#include "mt.h"
#include "motion.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define FONT_W 12
#define FONT_H 24
#define EXPECTED_ASSET_SIZE 1572408u
#define EXPECTED_POPULATED_SLOTS 2280u
#define TEST_CYCLE 60u

extern const uint8_t vf3_mt_asset[EXPECTED_ASSET_SIZE];

static pvr_ptr_t font_texture;
static char lines[14][96];
static MtFile mt_file;
static int asset_ok;
static int motion_eval_ok;

static int inspect_motion(unsigned motion_id, unsigned line)
{
    static const int32_t phases[] = {
        0x0100, 0x1000, 0x2000, 0x3B00
    };
    MtMotionView view;
    vf3_native_motion_stream stream;
    float values[VF3_NATIVE_MOTION_CHANNELS];
    unsigned op_count[4] = {0, 0, 0, 0};
    float minimum = 0.0f, maximum = 0.0f;
    unsigned sampled = 0;
    int rc;

    snprintf(lines[line], sizeof(lines[line]), "ID %u: LOAD FAIL", motion_id);
    rc = vf3_native_motion_load(&mt_file, motion_id, TEST_CYCLE,
                                &view, &stream);
    if (rc != 0) {
        snprintf(lines[line], sizeof(lines[line]),
                 "ID %u: LOAD ERROR %d", motion_id, rc);
        return 0;
    }

    for (unsigned i = 0; i < VF3_NATIVE_MOTION_CHANNELS; ++i) {
        if (view.ops[i] > 3u)
            return 0;
        ++op_count[view.ops[i]];
    }

    for (unsigned p = 0; p < sizeof(phases) / sizeof(phases[0]); ++p) {
        rc = vf3_native_motion_eval(&stream, phases[p], values);
        if (rc != 0) {
            snprintf(lines[line], sizeof(lines[line]),
                     "ID %u: EVAL ERROR %d", motion_id, rc);
            return 0;
        }
        for (unsigned ch = 0; ch < VF3_NATIVE_MOTION_CHANNELS; ++ch) {
            const float value = values[ch];
            if (!(value == value) || value > 1000000.0f ||
                value < -1000000.0f) {
                snprintf(lines[line], sizeof(lines[line]),
                         "ID %u: NON-FINITE CHANNEL", motion_id);
                return 0;
            }
            if (!sampled || value < minimum) minimum = value;
            if (!sampled || value > maximum) maximum = value;
            ++sampled;
        }
    }

    snprintf(lines[line], sizeof(lines[line]),
             "ID %u: PASS  WORD0 %u", motion_id,
             (unsigned)view.descriptor_word);
    snprintf(lines[line + 1], sizeof(lines[line + 1]),
             "OPS 0:%u 1:%u 2:%u 3:%u",
             op_count[0], op_count[1], op_count[2], op_count[3]);
    snprintf(lines[line + 2], sizeof(lines[line + 2]),
             "STREAM C/T/U %lu/%lu/%lu",
             (unsigned long)view.counts_size,
             (unsigned long)view.times_size,
             (unsigned long)view.tuples_size);
    snprintf(lines[line + 3], sizeof(lines[line + 3]),
             "RANGE %.2f .. %.2f", minimum, maximum);
    return 1;
}

static void inspect_asset(void)
{
    int first_ok, second_ok;

    snprintf(lines[0], sizeof(lines[0]), "ASSET BYTES: %lu",
             (unsigned long)EXPECTED_ASSET_SIZE);
    if (mt_parse(vf3_mt_asset, EXPECTED_ASSET_SIZE, &mt_file) != 0) {
        snprintf(lines[1], sizeof(lines[1]), "MT TABLE: PARSE FAIL");
        return;
    }

    snprintf(lines[1], sizeof(lines[1]), "POPULATED SLOTS: %lu",
             (unsigned long)mt_file.count);
    asset_ok = mt_file.count == EXPECTED_POPULATED_SLOTS &&
               mt_file.first_record_off == MT_PAYLOAD_FLOOR;
    first_ok = inspect_motion(1050u, 2u);
    second_ok = inspect_motion(2386u, 6u);
    motion_eval_ok = asset_ok && first_ok && second_ok;

    snprintf(lines[10], sizeof(lines[10]),
             "CYCLE TEST INPUT: %u", TEST_CYCLE);
    snprintf(lines[11], sizeof(lines[11]),
             "Q8 PHASES: 0100 1000 2000 3B00");
    snprintf(lines[12], sizeof(lines[12]),
             "SAMPLED CHANNELS: %u PER PHASE",
             VF3_NATIVE_MOTION_CHANNELS);
    snprintf(lines[13], sizeof(lines[13]),
             "CHANNEL-TO-BONE MAP STILL OPEN");
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
    float u2 = (ix + FONT_W) / 256.0f, v2 = (iy + FONT_H) / 256.0f;
    if (c <= ' ' || c >= 127) return;
    v.flags = PVR_CMD_VERTEX; v.z = 1.0f; v.u = u1; v.v = v2;
    v.x = x; v.y = y + FONT_H; v.argb = PVR_PACK_COLOR(1,1,1,1);
    v.oargb = 0; pvr_prim(&v, sizeof(v));
    v.x = x; v.y = y; v.v = v1; pvr_prim(&v, sizeof(v));
    v.x = x + FONT_W; v.y = y + FONT_H; v.u = u2; v.v = v2;
    pvr_prim(&v, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL; v.x = x + FONT_W; v.y = y; v.v = v1;
    pvr_prim(&v, sizeof(v));
}

static void draw_text(float x, float y, const char *s)
{
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

static void draw_screen(void)
{
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(20, 16, "VIRTUA FIGHTER 3 - REAL MT EVALUATION");
    draw_text(20, 48, asset_ok ? "MTAKIPAI TABLE: PASS" :
                                  "MTAKIPAI TABLE: FAIL");
    draw_text(20, 72, motion_eval_ok ? "63-CHANNEL EVALUATOR: PASS" :
                                       "63-CHANNEL EVALUATOR: FAIL");
    for (unsigned i = 0; i < 14; ++i)
        draw_text(20, 100 + i * 23, lines[i]);
    draw_text(20, 438, "SOURCE EVALUATED - BONE MAP STILL OPEN");
    pvr_list_finish();
    pvr_scene_finish();
}

int main(void)
{
    inspect_asset();
    pvr_init_defaults();
    pvr_set_bg_color(0.015f, 0.025f, 0.055f);
    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if (!font_texture) for (;;) thd_sleep(1000);
    font_init();
    for (;;) { draw_screen(); thd_sleep(16); }
    return motion_eval_ok ? 0 : 1;
}
