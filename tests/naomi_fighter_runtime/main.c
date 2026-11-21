/*
 * VF3 Naomi first native Akira model render.
 *
 * Parses the retail POL model packages, resolves the 18 selector-driven part
 * slots, builds the current inferred Akira bind pose, and submits the model
 * triangles directly to the Naomi PowerVR. Textures are intentionally omitted
 * in this first render milestone; the validated POL/TEX parser is upstream.
 */
#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pol.h"
#include "pol_mesh.h"
#include "motion_pose_kos.h"
#include "matrix.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define FONT_W 12
#define FONT_H 24
#define AKIRA_PART_COUNT 18u

extern const uint8_t vf3_mkao_aki_pol[];
extern const uint8_t vf3_mkao_aki_pol_end[];
extern const uint8_t vf3_rob_aki_pol[];
extern const uint8_t vf3_rob_aki_pol_end[];

typedef struct {
    uint8_t source; /* 1 = MKAO_AKI, 2 = ROB_AKI, 0 = empty */
    uint8_t slot;
    uint8_t mesh_count;
} AkiraPart;

static const AkiraPart parts[AKIRA_PART_COUNT] = {
    {2,50,3}, {1,3,7}, {2,51,1}, {2,52,1}, {2,53,2}, {0,0,0},
    {2,54,1}, {2,55,1}, {2,56,2}, {0,0,0}, {2,57,1}, {2,58,1},
    {2,59,1}, {2,60,3}, {2,36,2}, {2,61,1}, {2,62,3}, {2,35,2}
};

static PolScene head_scene, body_scene;
static float attachment[AKIRA_PART_COUNT][16];
static pvr_ptr_t font_texture;
static pvr_poly_hdr_t mesh_header;
static int model_ready;
static int pose_ready;
static char status_lines[5][80];

static const PolModel *find_model(const PolScene *scene, uint32_t slot)
{
    for (uint32_t i = 0; i < scene->model_count; ++i)
        if (scene->models[i].slot_index == slot)
            return &scene->models[i];
    return NULL;
}

static int load_model(void)
{
    const uint32_t head_size =
        (uint32_t)(vf3_mkao_aki_pol_end - vf3_mkao_aki_pol);
    const uint32_t body_size =
        (uint32_t)(vf3_rob_aki_pol_end - vf3_rob_aki_pol);
    int16_t zero_fields[VF3_NATIVE_POSE_FIELDS] = {0};

    if (pol_scene_parse(vf3_mkao_aki_pol, head_size, &head_scene) != 0)
        return -1;
    if (pol_scene_parse(vf3_rob_aki_pol, body_size, &body_scene) != 0)
        return -2;

    for (unsigned i = 0; i < AKIRA_PART_COUNT; ++i) {
        const AkiraPart *part = &parts[i];
        const PolScene *scene;
        const PolModel *model;
        if (!part->source) continue;
        scene = part->source == 1u ? &head_scene : &body_scene;
        model = find_model(scene, part->slot);
        if (!model || model->mesh_count < part->mesh_count ||
            model->first_mesh > scene->mesh_count ||
            part->mesh_count > scene->mesh_count - model->first_mesh)
            return -3;
    }

    if (vf3_native_motion_pose_fields(zero_fields,
            VF3_NATIVE_MOTION_RIG_AKIRA, attachment) != 0)
        return -4;
    pose_ready = 1;
    model_ready = 1;
    snprintf(status_lines[0], sizeof status_lines[0],
             "MKAO_AKI: %lu MESHES", (unsigned long)head_scene.mesh_count);
    snprintf(status_lines[1], sizeof status_lines[1],
             "ROB_AKI: %lu MESHES", (unsigned long)body_scene.mesh_count);
    snprintf(status_lines[2], sizeof status_lines[2],
             "PART SELECTORS: 18 / 18");
    snprintf(status_lines[3], sizeof status_lines[3],
             "POSE: INFERRED AKIRA BIND RIG");
    snprintf(status_lines[4], sizeof status_lines[4],
             "POWER VR: UNTEXTURED GEOMETRY");
    return 0;
}

static float clamp01(float x)
{
    if (!(x > 0.0f)) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static uint32_t mesh_color(const PolMesh *mesh)
{
    float r = clamp01(mesh->color[0]);
    float g = clamp01(mesh->color[1]);
    float b = clamp01(mesh->color[2]);
    if (r == 0.0f && g == 0.0f && b == 0.0f)
        r = g = b = 0.82f;
    return PVR_PACK_COLOR(1.0f, r, g, b);
}

static void draw_mesh(const PolScene *scene, const PolMesh *mesh,
                      const float matrix[16], const float root[3])
{
    const uint32_t color = mesh_color(mesh);
    if (mesh->index_count < 3u ||
        mesh->first_index > scene->index_count ||
        mesh->index_count > scene->index_count - mesh->first_index)
        return;

    pvr_prim(&mesh_header, sizeof(mesh_header));
    for (uint32_t i = 0; i + 2u < mesh->index_count; i += 3u) {
        for (unsigned corner = 0; corner < 3u; ++corner) {
            const uint32_t index = scene->indices[mesh->first_index + i + corner];
            const PolMeshVertex *src;
            float point[3], world[3];
            pvr_vertex_t v;
            if (index >= scene->vertex_count) continue;
            src = &scene->vertices[index];
            point[0] = src->x; point[1] = src->y; point[2] = src->z;
            vf3_native_matrix_transform_point(matrix, point, world);
            world[0] -= root[0];
            world[1] -= root[1];
            world[2] -= root[2];

            memset(&v, 0, sizeof v);
            v.flags = corner == 2u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = 320.0f + world[0] * 205.0f;
            v.y = 424.0f -
                (world[1] * 0.9759001f + world[2] * 0.2182179f) * 205.0f;
            v.z = 8.0f + (world[2] + 2.0f) * 0.1f;
            v.argb = color;
            v.oargb = 0;
            pvr_prim(&v, sizeof v);
        }
    }
}

static void draw_akira(void)
{
    const float root[3] = {
        attachment[0][12], attachment[0][13], attachment[0][14]
    };
    for (unsigned i = 0; i < AKIRA_PART_COUNT; ++i) {
        const AkiraPart *part = &parts[i];
        const PolScene *scene;
        const PolModel *model;
        if (!part->source) continue;
        scene = part->source == 1u ? &head_scene : &body_scene;
        model = find_model(scene, part->slot);
        if (!model) continue;
        for (uint32_t m = 0; m < part->mesh_count; ++m) {
            const PolMesh *mesh = &scene->meshes[model->first_mesh + m];
            draw_mesh(scene, mesh, attachment[i], root);
        }
    }
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
    memset(&v, 0, sizeof v);
    v.flags = PVR_CMD_VERTEX; v.z = 1.0f; v.u = u1; v.v = v2;
    v.x = x; v.y = y + FONT_H; v.argb = PVR_PACK_COLOR(1,1,1,1);
    pvr_prim(&v, sizeof v);
    v.x = x; v.y = y; v.v = v1; pvr_prim(&v, sizeof v);
    v.x = x + FONT_W; v.y = y + FONT_H; v.u = u2; v.v = v2;
    pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.x = x + FONT_W; v.y = y; v.v = v1;
    pvr_prim(&v, sizeof v);
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
    pvr_prim(&poly, sizeof poly);
    for (size_t i = 0; s[i]; ++i) {
        draw_char(x, y, (unsigned char)s[i]);
        x += FONT_W;
    }
}

static void draw_screen(void)
{
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    if (model_ready && pose_ready)
        draw_akira();
    pvr_list_finish();

    pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(16, 12, "VF3 NAOMI - AKIRA MODEL");
    draw_text(16, 38, model_ready ? "POL GEOMETRY: PASS" :
                                      "POL GEOMETRY: FAIL");
    draw_text(16, 62, pose_ready ? "BIND POSE: PASS" :
                                     "BIND POSE: FAIL");
    for (unsigned i = 0; i < 5; ++i)
        draw_text(16, 92 + i * 22, status_lines[i]);
    draw_text(16, 438, "STATIC MODEL - TEXTURES AND LIVE MOTION NEXT");
    pvr_list_finish();
    pvr_scene_finish();
}

int main(void)
{
    pvr_poly_cxt_t cxt;
    const int load_rc = load_model();
    if (load_rc != 0) {
        snprintf(status_lines[0], sizeof status_lines[0],
                 "MODEL LOAD ERROR %d", load_rc);
    }

    pvr_init_defaults();
    pvr_set_bg_color(0.015f, 0.025f, 0.055f);
    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    cxt.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&mesh_header, &cxt);

    font_texture = pvr_mem_malloc(256 * 256 * 2);
    if (!font_texture) for (;;) thd_sleep(1000);
    font_init();

    for (;;) {
        draw_screen();
        thd_sleep(16);
    }
    return 0;
}
