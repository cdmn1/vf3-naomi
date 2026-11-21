#include "pol.h"
#include "pol_mesh.h"
#include "pvr_texture.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load_file(const char *path, uint8_t **out, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) <= 0 ||
        (unsigned long)n > UINT32_MAX || fseek(f, 0, SEEK_SET)) {
        fclose(f); return -1;
    }
    *out = (uint8_t *)malloc((size_t)n);
    if (!*out || fread(*out, 1, (size_t)n, f) != (size_t)n) {
        free(*out); *out = NULL; fclose(f); return -1;
    }
    fclose(f); *size = (uint32_t)n; return 0;
}

static const PolModel *find_model(const PolScene *s, uint32_t slot)
{
    for (uint32_t i = 0; i < s->model_count; ++i)
        if (s->models[i].slot_index == slot) return &s->models[i];
    return NULL;
}

static int audit_pol(const char *name, const char *tex_name,
                     const uint32_t (*required)[2], size_t required_count)
{
    uint8_t *pol = NULL, *tex = NULL;
    uint32_t pol_size = 0, tex_size = 0, desc_count = 0;
    PolHeader h;
    PolTextureDesc *desc = NULL;
    PolScene scene = {0};
    int fail = 0;
    if (load_file(name, &pol, &pol_size) || load_file(tex_name, &tex, &tex_size)) {
        fprintf(stderr, "ASSET LOAD FAIL: %s / %s\n", name, tex_name);
        fail = 1; goto done;
    }
    if (pol_header(pol, pol_size, &h) ||
        pol_textures(pol, pol_size, NULL, 0, &desc_count) != -2 ||
        pol_scene_parse(pol, pol_size, &scene)) {
        fprintf(stderr, "POL PARSE FAIL: %s\n", name);
        fail = 1; goto done;
    }
    if (desc_count != h.texture_count || !scene.model_count ||
        !scene.mesh_count || !scene.vertex_count || !scene.index_count) {
        fprintf(stderr, "POL EMPTY/HEADER MISMATCH: %s\n", name);
        fail = 1; goto done;
    }
    desc = (PolTextureDesc *)calloc(desc_count, sizeof *desc);
    if (desc_count && (!desc || pol_textures(pol, pol_size, desc, desc_count))) {
        fail = 1; goto done;
    }
    for (uint32_t i = 0; i < desc_count; ++i) {
        size_t bytes = 0, pixels_n = (size_t)desc[i].width * desc[i].height;
        uint32_t *pixels;
        if (vf3_pvr_texture_payload_size(&desc[i], &bytes) ||
            (uint64_t)desc[i].data_offset + bytes > tex_size ||
            pixels_n > SIZE_MAX / sizeof(uint32_t)) {
            fprintf(stderr, "TEXTURE BOUNDS/FORMAT FAIL: %s texture %u\n", name, i);
            fail = 1; goto done;
        }
        pixels = (uint32_t *)malloc(pixels_n * sizeof(uint32_t));
        if (!pixels || vf3_pvr_texture_decode(tex, tex_size, &desc[i],
                                               pixels, pixels_n)) {
            free(pixels);
            fprintf(stderr, "TEXTURE DECODE FAIL: %s texture %u\n", name, i);
            fail = 1; goto done;
        }
        free(pixels);
    }
    for (size_t i = 0; i < required_count; ++i) {
        const PolModel *m = find_model(&scene, required[i][0]);
        if (!m || m->mesh_count < required[i][1]) {
            fprintf(stderr, "PART SLOT FAIL: %s slot %u needs %u meshes\n",
                    name, required[i][0], required[i][1]);
            fail = 1; goto done;
        }
    }
    for (uint32_t i = 0; i < scene.mesh_count; ++i) {
        const PolMesh *m = &scene.meshes[i];
        if ((m->texture_index != POL_TEXTURE_NONE &&
             m->texture_index >= desc_count) ||
            m->first_index > scene.index_count ||
            m->index_count > scene.index_count - m->first_index ||
            m->index_count % 3u) {
            fprintf(stderr, "MESH RANGE/MATERIAL FAIL: %s mesh %u\n", name, i);
            fail = 1; goto done;
        }
    }
    printf("%s: %u bytes; %u models; %u meshes; %u vertices; %u indices; %u textures\n",
           name, pol_size, scene.model_count, scene.mesh_count,
           scene.vertex_count, scene.index_count, desc_count);
done:
    free(desc); pol_scene_free(&scene); free(pol); free(tex);
    return fail;
}

int main(void)
{
    static const uint32_t head_parts[][2] = {{3, 7}};
    static const uint32_t body_parts[][2] = {
        {50,3},{51,1},{52,1},{53,2},{54,1},{55,1},{56,2},
        {57,1},{58,1},{59,1},{60,3},{61,1},{62,3},{36,2},{35,2}
    };
    int fail = 0;
    puts("VF3 AKIRA POL/TEX ASSET PREFLIGHT");
    fail += audit_pol("MKAO_AKI.POL", "MKAO_AKI.TEX",
                      head_parts, sizeof head_parts / sizeof head_parts[0]);
    fail += audit_pol("ROB_AKI.POL", "ROB_AKI.TEX",
                      body_parts, sizeof body_parts / sizeof body_parts[0]);
    puts(fail ? "AKIRA ASSET PREFLIGHT: FAIL" : "AKIRA ASSET PREFLIGHT: PASS");
    return fail ? 1 : 0;
}
