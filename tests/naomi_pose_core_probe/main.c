#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>
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
#define RAM_SIZE 0x400000u
#define REGION_BASE 0x0C200000u
#define REGION_END  0x0C20F000u
#define OBJ         0x0C204000u
#define ARG         0x0C20A000u
#define CODEBLK     0x0C209000u
#define CODEBLK2    0x0C20B000u
#define SP          0x0C20FF00u
#define KIND_REC    0x0C10D3C4u
#define KIND_TAB2   0x0C10DABCu
#define STACK_CTL   0x0C19D2E4u
#define STAGE_WORD  0x0C29BCCCu
#define MACL        0x0DEADBEEFu

static float ram[RAM_SIZE / sizeof(float)];
static char lines[8][64];
static pvr_ptr_t font_texture;
static int pass_count, fail_count;

static uint8_t *mem_at(uint32_t addr)
{
    return (uint8_t *)ram + (addr - RAM_BASE);
}

static void w8(uint32_t addr, uint8_t v)
{
    *mem_at(addr) = v;
}

static void w16(uint32_t addr, uint16_t v)
{
    uint8_t *p = mem_at(addr);
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void w32(uint32_t addr, uint32_t v)
{
    uint8_t *p = mem_at(addr);
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void wf(uint32_t addr, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    w32(addr, bits);
}

static void check(int ok, const char *name)
{
    snprintf(lines[pass_count + fail_count], sizeof lines[0], "%s: %s",
             name, ok ? "PASS" : "FAIL");
    if (ok) ++pass_count; else ++fail_count;
}

static int finite_block(uint32_t addr)
{
    for (unsigned i = 0; i < 16; ++i) {
        float v;
        memcpy(&v, mem_at(addr + i * 4u), sizeof v);
        if (!isfinite(v)) return 0;
    }
    return 1;
}

static void seed_region(void)
{
    for (uint32_t addr = REGION_BASE; addr < REGION_END; addr += 4u)
        w32(addr, 0x3F800000u ^ (addr & 0x7FFu));
}

static void seed_stack_ctl(void)
{
    w32(STACK_CTL, (16u << 16) | 1u);
    w32(STACK_CTL + 4u, 0x0C19D400u);
    w32(STACK_CTL + 8u, 0x0C19D440u);
    w32(STAGE_WORD, 0);
}

static void seed_case(uint8_t kind, uint8_t selector)
{
    seed_region();
    seed_stack_ctl();

    w8(OBJ + 97u, kind);
    w8(OBJ + 0x420Bu, selector);
    w16(OBJ + 30u, 0x0600u);
    w16(OBJ + 0x1F50u, 0x1000u);
    w16(OBJ + 0x1F52u, 0x2000u);
    w16(OBJ + 0x1F54u, 0x3000u);
    w16(OBJ + 0x22E4u, 0x1234u);
    wf(OBJ + 0x1EFCu, 1.0f);
    wf(OBJ + 0x1F00u, 2.0f);
    wf(OBJ + 0x1F04u, 3.0f);
    wf(OBJ + 0x208Cu, 1.5f);
    wf(OBJ + 0x1F08u, -0.5f);
    wf(OBJ + 0x2090u, 2.5f);
    wf(OBJ + 0x1F0Cu, 0.5f);
    wf(OBJ + 0x2094u, 3.5f);
    wf(OBJ + 0x1F10u, -1.0f);
    wf(OBJ + 0x20B4u, 1.25f);
    wf(OBJ + 0x20C0u, -0.75f);
    w16(OBJ + 0x22E8u, 0x0A00u);
    w16(OBJ + 0x22EAu, 0x0B00u);

    wf(OBJ + 0xCCu, 0.1f);  wf(OBJ + 0xD4u, 4.0f);
    wf(OBJ + 0xDCu, 0.2f);  wf(OBJ + 0xE4u, 5.0f);
    wf(OBJ + 0xFCu, 0.3f);  wf(OBJ + 0x104u, 6.0f);
    wf(OBJ + 0x10Cu, 0.4f); wf(OBJ + 0x114u, 7.0f);
    wf(OBJ + 0x138u, 0.5f); wf(OBJ + 0x140u, 8.0f);
    wf(OBJ + 0x148u, 0.6f); wf(OBJ + 0x150u, 9.0f);
    wf(OBJ + 0x15Cu, 0.7f); wf(OBJ + 0x164u, 10.0f);
    wf(OBJ + 0x16Cu, 0.8f); wf(OBJ + 0x174u, 11.0f);

    {
        const float code1[18] = {
            1.0f,2.0f,-3.0f, 0.9f,1.1f,1.2f,
            2.0f,-2.0f,0.5f, 3.0f,-3.0f,0.25f,
            4.0f,-4.0f,0.125f, 5.0f,-5.0f,0.0625f
        };
        const float code2[18] = {
            9.0f,8.0f,-7.0f, 7.0f,6.0f,5.0f,
            4.0f,-4.0f,0.75f, 3.0f,-3.0f,0.875f,
            2.0f,-2.0f,0.9375f, 1.0f,-1.0f,0.96875f
        };
        for (unsigned i = 0; i < 18; ++i) {
            wf(CODEBLK + i * 4u, code1[i]);
            wf(CODEBLK2 + i * 4u, code2[i]);
        }
    }

    for (unsigned i = 0; i < 16; ++i)
        w32(KIND_TAB2 + i * 4u, CODEBLK2);
    w32(KIND_TAB2 + 8u, CODEBLK);

    {
        const uint32_t kr = KIND_REC + 108u * (uint32_t)kind;
        const float kf[13] = {
            0.1f,0.2f,0.3f,0.4f,0.5f,0.6f,0.7f,
            0.8f,0.9f,1.0f,1.1f,1.2f,1.3f
        };
        for (unsigned i = 0; i < 7; ++i)
            wf(kr + i * 4u, kf[i]);
        wf(kr + 36u, kf[7]);
        wf(kr + 48u, kf[8]);
        wf(kr + 52u, kf[9]);
        wf(kr + 56u, kf[10]);
        wf(kr + 60u, kf[11]);
        wf(kr + 84u, kf[12]);
    }

    wf(ARG, 0.0f);
    wf(ARG + 12u, 0.0f);
    wf(ARG + 24u, 0.0f);
    wf(ARG + 0x9Cu, 0.0f);
    wf(ARG + 36u, 100.0f);
    wf(ARG + 96u, 200.0f);
    wf(ARG + 0xA8u, 300.0f);
    wf(ARG + 0xFCu, 400.0f);
    w32(SP - 8u, MACL);
}

static int run_orchestration_case(uint8_t kind, uint8_t selector)
{
    static const uint32_t output_blocks[4] = {
        OBJ + 0x200u, OBJ + 0x300u, OBJ + 0x480u, OBJ + 0x540u
    };
    uint8_t before[4][64];
    vf3_native_aim aim;

    memset(ram, 0, sizeof ram);
    seed_case(kind, selector);
    for (unsigned i = 0; i < 4; ++i)
        memcpy(before[i], mem_at(output_blocks[i]), sizeof before[i]);

    memset(&aim, 0, sizeof aim);
    aim.ram = ram;
    aim.ram_base = RAM_BASE;
    aim.f = SP;
    aim.pr = 0x0C0ABCDEu;
    aim.sr = 0x60000000u;
    aim.fpscr = 0x00040001u;

    vf3_native_pose_orchestration(&aim, SP, OBJ, ARG);

    int changed = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (memcmp(before[i], mem_at(output_blocks[i]), sizeof before[i]) != 0)
            changed = 1;
    }

    int finite = 1;
    for (unsigned i = 0; i < 4; ++i)
        finite &= finite_block(output_blocks[i]);

    int matrix_finite = 1;
    for (unsigned i = 0; i < 16; ++i)
        matrix_finite &= isfinite(aim.xm[i]);

    return changed && finite && matrix_finite &&
           aim.sp == 0 && aim.r[15] == SP + 28u &&
           aim.pr == 0x0C0ABCDEu;
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
    check(run_orchestration_case(3, 3), "POSE KIND 3");
    check(run_orchestration_case(12, 0), "POSE KIND 12 BRANCH");
    check(run_orchestration_case(21, 5), "POSE KIND 21");
    check(run_orchestration_case(0, 1), "POSE KIND 0");

    pvr_init_defaults();
    pvr_set_bg_color(0.015f, 0.025f, 0.055f);
    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if (!font_texture) for (;;) thd_sleep(1000);
    font_init();

    for (;;) {
        char summary[64];
        snprintf(summary, sizeof summary, "RESULT: %d PASS / %d FAIL",
                 pass_count, fail_count);
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_TR_POLY);
        draw_text(16, 12, "VF3 NAOMI - FULL POSE ORCHESTRATION");
        for (int i = 0; i < pass_count + fail_count; ++i)
            draw_text(16, 70 + i * 38, lines[i]);
        draw_text(16, 420, summary);
        pvr_list_finish();
        pvr_scene_finish();
        thd_sleep(16);
    }
    return 0;
}
