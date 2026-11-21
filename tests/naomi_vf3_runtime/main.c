/*
 * VF3 Naomi real MT asset table probe.
 *
 * Embeds the user's MTAKIPAI.BIN asset in the Naomi ROM and validates its
 * 8,880-entry slot table. Record extents are derived from sorted unique
 * offsets, not slot order: the real file contains out-of-order and duplicate
 * offsets, so the upstream simple iterator is not safe for this asset.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

extern const uint8_t vf3_mt_asset[EXPECTED_ASSET_SIZE];

static pvr_ptr_t font_texture;
static uint32_t offsets[MT_SLOTS];
static uint32_t unique_offsets[MT_SLOTS];
static uint32_t populated, invalid, inversions, duplicates, records;
static uint32_t first_offset, last_offset;
static int asset_ok;
static char summary[8][96];

static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int cmp_u32(const void *a, const void *b) {
    uint32_t x=*(const uint32_t *)a, y=*(const uint32_t *)b;
    return (x>y)-(x<y);
}

static void inspect_asset(void) {
    const uint32_t size=EXPECTED_ASSET_SIZE;
    uint32_t previous=0, n=0;
    if (size!=EXPECTED_ASSET_SIZE || size<MT_TABLE_BYTES) return;

    for (uint32_t slot=0; slot<MT_SLOTS; ++slot) {
        uint32_t off=rd32le(vf3_mt_asset+slot*4u);
        if (!off) continue;
        if (off<MT_TABLE_BYTES || off>=size) { ++invalid; continue; }
        if (n && off<previous) ++inversions;
        previous=off;
        offsets[n++]=off;
    }
    populated=n;
    if (!n || invalid) return;

    qsort(offsets,n,sizeof(offsets[0]),cmp_u32);
    for (uint32_t i=0;i<n;++i)
        if (!i || offsets[i]!=offsets[i-1])
            unique_offsets[records++]=offsets[i];
    duplicates=populated-records;
    first_offset=unique_offsets[0];
    last_offset=unique_offsets[records-1];

    snprintf(summary[0],sizeof(summary[0]),"ASSET BYTES: %lu",(unsigned long)size);
    snprintf(summary[1],sizeof(summary[1]),"POPULATED SLOTS: %lu / %u",(unsigned long)populated,MT_SLOTS);
    snprintf(summary[2],sizeof(summary[2]),"UNIQUE FILE OFFSETS: %lu",(unsigned long)records);
    snprintf(summary[3],sizeof(summary[3]),"DUPLICATE POINTERS: %lu",(unsigned long)duplicates);
    snprintf(summary[4],sizeof(summary[4]),"SLOT ORDER INVERSIONS: %lu",(unsigned long)inversions);
    snprintf(summary[5],sizeof(summary[5]),"PAYLOAD: %06X .. %06X",(unsigned)first_offset,(unsigned)last_offset);
    snprintf(summary[6],sizeof(summary[6]),"MOTION 1050 SLOTS: %u-%u",(1050u-1u)*3u,(1050u-1u)*3u+2u);
    snprintf(summary[7],sizeof(summary[7]),"MOTION 2386 SLOTS: %u-%u",(2386u-1u)*3u,(2386u-1u)*3u+2u);
    asset_ok=1;
}

static void font_init(void) {
    uint16_t *vram=(uint16_t *)font_texture;
    for(int y=0;y<8;++y) {
        for(int x=0;x<16;++x) {
            minifont_draw(vram,256,(uint32_t)(y*16+x));
            vram+=16;
        }
        vram+=23*256;
    }
}

static void draw_char(float x,float y,unsigned char c) {
    pvr_vertex_t v;
    int ix=(c%16)*16, iy=(c/16)*24;
    float u1=ix/256.0f, v1=iy/256.0f;
    float u2=(ix+FONT_W)/256.0f, v2=(iy+FONT_H)/256.0f;
    if(c<=' '||c>=127) return;
    v.flags=PVR_CMD_VERTEX; v.z=1.0f; v.u=u1; v.v=v2; v.x=x; v.y=y+FONT_H;
    v.argb=PVR_PACK_COLOR(1,1,1,1); v.oargb=0; pvr_prim(&v,sizeof(v));
    v.x=x; v.y=y; v.v=v1; pvr_prim(&v,sizeof(v));
    v.x=x+FONT_W; v.y=y+FONT_H; v.u=u2; v.v=v2; pvr_prim(&v,sizeof(v));
    v.flags=PVR_CMD_VERTEX_EOL; v.x=x+FONT_W; v.y=y; v.v=v1; pvr_prim(&v,sizeof(v));
}

static void draw_text(float x,float y,const char *s) {
    pvr_poly_cxt_t cxt; pvr_poly_hdr_t poly;
    if(!s||!s[0]) return;
    pvr_poly_cxt_txr(&cxt,PVR_LIST_TR_POLY,
        PVR_TXRFMT_ARGB1555|PVR_TXRFMT_NONTWIDDLED,
        256,256,font_texture,PVR_FILTER_NONE);
    pvr_poly_compile(&poly,&cxt); pvr_prim(&poly,sizeof(poly));
    for(size_t i=0;s[i];++i) { draw_char(x,y,(unsigned char)s[i]); x+=FONT_W; }
}

static void draw_screen(void) {
    pvr_wait_ready(); pvr_scene_begin(); pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(24,24,"VIRTUA FIGHTER 3 - NAOMI");
    draw_text(24,64,asset_ok?"REAL MTAKIPAI.BIN: PASS":"REAL MTAKIPAI.BIN: FAIL");
    for(unsigned i=0;i<8;++i) draw_text(24,112+i*32,summary[i]);
    draw_text(24,408,"REAL GAME ASSET EMBEDDED - TABLE ONLY");
    pvr_list_finish(); pvr_scene_finish();
}

int main(void) {
    inspect_asset();
    pvr_init_defaults();
    pvr_set_bg_color(0.015f,0.025f,0.055f);
    font_texture=pvr_mem_malloc(256*256*2);
    if(!font_texture) for(;;) thd_sleep(1000);
    font_init();
    for(;;) { draw_screen(); thd_sleep(16); }
    return asset_ok?0:1;
}
