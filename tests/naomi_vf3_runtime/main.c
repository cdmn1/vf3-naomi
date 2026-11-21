/*
 * VF3 Naomi real MT motion-component probe.
 *
 * Embeds the verified MTAKIPAI.BIN asset and inspects the three slot
 * components selected by the motion IDs observed in the two actor contexts.
 * Component boundaries are found by the next greater unique file offset,
 * never by slot order. This validates real game data without pretending
 * that the component semantics or playback path are already understood.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

#define FONT_W 12
#define FONT_H 24
#define MT_SLOTS 8880u
#define MT_TABLE_BYTES (MT_SLOTS * 4u)
#define EXPECTED_ASSET_SIZE 1572408u
#define MT_PAYLOAD_FLOOR 0x8AC0u

extern const uint8_t vf3_mt_asset[EXPECTED_ASSET_SIZE];

static pvr_ptr_t font_texture;
static char lines[15][96];
static int asset_ok;

static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t slot_offset(unsigned slot) {
    if (slot >= MT_SLOTS) return 0;
    return rd32le(vf3_mt_asset + slot * 4u);
}

static uint32_t next_greater_offset(uint32_t off) {
    uint32_t next = EXPECTED_ASSET_SIZE;
    for (unsigned i = 0; i < MT_SLOTS; ++i) {
        uint32_t v = slot_offset(i);
        if (v > off && v < next && v < EXPECTED_ASSET_SIZE)
            next = v;
    }
    return next;
}

static int inspect_motion(unsigned motion_id, unsigned line) {
    const unsigned first_slot = (motion_id - 1u) * 3u;
    uint32_t off[3], len[3];
    for (unsigned i = 0; i < 3; ++i) {
        off[i] = slot_offset(first_slot + i);
        if (off[i] < MT_PAYLOAD_FLOOR || off[i] >= EXPECTED_ASSET_SIZE)
            return 0;
        uint32_t next = next_greater_offset(off[i]);
        if (next <= off[i] || next > EXPECTED_ASSET_SIZE)
            return 0;
        len[i] = next - off[i];
    }

    const uint8_t *descriptor = vf3_mt_asset + off[0];
    const uint32_t reference = rd32le(descriptor + 8);
    if (len[0] != 28u || len[1] != 16u || len[2] != 404u ||
        reference < MT_PAYLOAD_FLOOR || reference >= EXPECTED_ASSET_SIZE)
        return 0;

    const uint8_t *payload = vf3_mt_asset + off[2];
    float min_value = 0.0f, max_value = 0.0f;
    unsigned valid_floats = 0;
    for (unsigned i = 0; i < len[2] / 4u; ++i) {
        float value;
        memcpy(&value, payload + i * 4u, sizeof(value));
        if (!(value == value) || value > 1000000.0f || value < -1000000.0f)
            continue;
        if (!valid_floats || value < min_value) min_value = value;
        if (!valid_floats || value > max_value) max_value = value;
        ++valid_floats;
    }
    if (valid_floats != 101u) return 0;

    snprintf(lines[line], sizeof(lines[line]),
             "ID %u  SLOTS %u-%u", motion_id, first_slot, first_slot + 2u);
    snprintf(lines[line + 1], sizeof(lines[line + 1]),
             "A %06lX/28  B %06lX/16",
             (unsigned long)off[0], (unsigned long)off[1]);
    snprintf(lines[line + 2], sizeof(lines[line + 2]),
             "C %06lX/404  101 FLOATS",
             (unsigned long)off[2]);
    snprintf(lines[line + 3], sizeof(lines[line + 3]),
             "REF %06lX  HDR %08lX %08lX",
             (unsigned long)reference,
             (unsigned long)rd32le(descriptor),
             (unsigned long)rd32le(descriptor + 4));
    snprintf(lines[line + 4], sizeof(lines[line + 4]),
             "RANGE %.1f .. %.1f", min_value, max_value);
    return 1;
}

static void inspect_asset(void) {
    if (EXPECTED_ASSET_SIZE < MT_TABLE_BYTES) return;
    unsigned populated = 0, invalid = 0;
    for (unsigned i = 0; i < MT_SLOTS; ++i) {
        uint32_t off = slot_offset(i);
        if (!off) continue;
        ++populated;
        if (off < MT_PAYLOAD_FLOOR || off >= EXPECTED_ASSET_SIZE)
            ++invalid;
    }
    snprintf(lines[0], sizeof(lines[0]), "ASSET BYTES: %lu",
             (unsigned long)EXPECTED_ASSET_SIZE);
    snprintf(lines[1], sizeof(lines[1]), "POPULATED SLOTS: %u", populated);
    if (invalid || populated != 2280u) return;
    if (!inspect_motion(1050u, 3u)) return;
    if (!inspect_motion(2386u, 9u)) return;
    asset_ok = 1;
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

static void draw_text(float x, float y, const char *s) {
    pvr_poly_cxt_t cxt; pvr_poly_hdr_t poly;
    if (!s || !s[0]) return;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
        PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
        256, 256, font_texture, PVR_FILTER_NONE);
    pvr_poly_compile(&poly, &cxt); pvr_prim(&poly, sizeof(poly));
    for (size_t i = 0; s[i]; ++i) {
        draw_char(x, y, (unsigned char)s[i]);
        x += FONT_W;
    }
}

static void draw_screen(void) {
    pvr_wait_ready(); pvr_scene_begin(); pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(20, 16, "VIRTUA FIGHTER 3 - REAL MT DATA");
    draw_text(20, 48, asset_ok ? "MTAKIPAI: MOTION DATA PASS" :
                                  "MTAKIPAI: MOTION DATA FAIL");
    for (unsigned i = 0; i < 14; ++i)
        draw_text(20, 76 + i * 25, lines[i]);
    draw_text(20, 438, "COMPONENTS VALIDATED - PLAYBACK NOT YET MAPPED");
    pvr_list_finish(); pvr_scene_finish();
}

int main(void) {
    inspect_asset();
    pvr_init_defaults();
    pvr_set_bg_color(0.015f, 0.025f, 0.055f);
    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if (!font_texture) for (;;) thd_sleep(1000);
    font_init();
    for (;;) { draw_screen(); thd_sleep(16); }
    return asset_ok ? 0 : 1;
}
