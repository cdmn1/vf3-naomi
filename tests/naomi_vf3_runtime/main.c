/*
 * VF3 Naomi motion lab.
 *
 * Executes the upstream VF3 SH-4 motion-channel evaluator on Naomi and
 * displays its 63-float output as an animated diagnostic pose.
 * The motion fixture is synthetic: no commercial game data is included.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

typedef struct VF3MtTask {
    const uint8_t *hdr;
    uint16_t cycle;
    const uint8_t *ram;
    uint32_t ram_base;
} VF3MtTask;

extern int vf3_mt_eval_frame(const VF3MtTask *task, int32_t phase, float **out_pp);

#define FONT_W 12
#define FONT_H 24
#define JOINTS 21
#define RAM_BASE 0x0C100000u

static pvr_ptr_t font_texture;
static uint8_t motion_ram[0x1000];
static uint8_t motion_header[12 + 63];
static float pose[63];
static VF3MtTask task;
static int evaluator_ok;
static unsigned frame_no;

static const float pose_a[JOINTS][2] = {
    { 0.00f,-1.45f}, { 0.00f,-1.08f},
    {-0.34f,-0.98f}, { 0.34f,-0.98f},
    {-0.58f,-0.58f}, { 0.58f,-0.58f},
    {-0.72f,-0.12f}, { 0.72f,-0.12f},
    { 0.00f,-0.12f}, {-0.22f, 0.02f},
    { 0.22f, 0.02f}, {-0.26f, 0.55f},
    { 0.26f, 0.55f}, {-0.30f, 1.12f},
    { 0.30f, 1.12f}, {-0.16f,-0.72f},
    { 0.16f,-0.72f}, {-0.42f,-0.28f},
    { 0.42f,-0.28f}, {-0.20f, 0.32f},
    { 0.20f, 0.32f}
};

static const uint8_t links[][2] = {
    {0,1},{1,2},{1,3},{2,4},{4,6},{3,5},{5,7},
    {1,8},{8,9},{8,10},{9,11},{11,13},{10,12},{12,14},
    {2,15},{3,16},{15,17},{16,18},{9,19},{10,20}
};

static void put32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

static int channel_is_flipped(unsigned i) {
    if (i >= 2 && i <= 20 && (i - 2) % 3 == 0) return 1;
    if (i >= 21 && (i - 21) % 3 == 0) return 1;
    return 0;
}

static void build_fixture(void) {
    uint8_t *counts = motion_ram + 0x100;
    uint8_t *times = motion_ram + 0x200;
    float *tuples = (float *)(motion_ram + 0x300);

    put32(motion_header + 0, RAM_BASE + 0x100);
    put32(motion_header + 4, RAM_BASE + 0x200);
    put32(motion_header + 8, RAM_BASE + 0x300);

    for (unsigned i=0; i<63; ++i) {
        unsigned joint=i/3, axis=i%3;
        float a = axis < 2 ? pose_a[joint][axis] : 0.0f;
        float b = a;

        /* A looped guard/ready-style diagnostic pose: arms lift, then return. */
        if (joint == 4 || joint == 6 || joint == 17) b -= 0.34f;
        if (joint == 5 || joint == 7 || joint == 18) b -= 0.18f;
        if (joint == 11 || joint == 13 || joint == 19) b += 0.10f;
        if (joint == 12 || joint == 14 || joint == 20) b += 0.06f;
        if (joint == 1 || joint == 8) b += 0.04f;

        counts[i]=3;
        times[i*3+0]=1;
        times[i*3+1]=31;
        times[i*3+2]=61;
        motion_header[12+i]=2; /* scalar keyframed channel */

        if (channel_is_flipped(i)) { a=-a; b=-b; }
        tuples[i*5+0]=0.0f;
        tuples[i*5+1]=a;
        tuples[i*5+2]=b;
        tuples[i*5+3]=a;
        tuples[i*5+4]=0.0f;
    }

    task.hdr=motion_header;
    task.cycle=61;
    task.ram=motion_ram;
    task.ram_base=RAM_BASE;
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
    v.flags=PVR_CMD_VERTEX; v.z=1.0f;
    v.u=u1; v.v=v2; v.x=x; v.y=y+FONT_H;
    v.argb=PVR_PACK_COLOR(1,1,1,1); v.oargb=0; pvr_prim(&v,sizeof(v));
    v.x=x; v.y=y; v.v=v1; pvr_prim(&v,sizeof(v));
    v.x=x+FONT_W; v.y=y+FONT_H; v.u=u2; v.v=v2; pvr_prim(&v,sizeof(v));
    v.flags=PVR_CMD_VERTEX_EOL; v.x=x+FONT_W; v.y=y; v.v=v1;
    pvr_prim(&v,sizeof(v));
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

static void draw_quad(float x1,float y1,float x2,float y2,float width,uint32_t color) {
    pvr_vertex_t v;
    float dx=x2-x1, dy=y2-y1;
    float len=dx*dx+dy*dy;
    float px,py;
    if(len<0.0001f) return;
    /* Reciprocal square root is avoided; this is only a debug overlay. */
    float inv=1.0f;
    while(inv*inv*len < 1.0f) inv*=2.0f;
    while(inv*inv*len > 1.0f) inv*=0.5f;
    px=-dy*inv*width*0.5f; py=dx*inv*width*0.5f;
    v.z=1.0f; v.u=v.v=0; v.argb=color; v.oargb=0;
    v.flags=PVR_CMD_VERTEX; v.x=x1+px; v.y=y1+py; pvr_prim(&v,sizeof(v));
    v.x=x1-px; v.y=y1-py; pvr_prim(&v,sizeof(v));
    v.x=x2+px; v.y=y2+py; pvr_prim(&v,sizeof(v));
    v.flags=PVR_CMD_VERTEX_EOL; v.x=x2-px; v.y=y2-py; pvr_prim(&v,sizeof(v));
}

static void draw_pose(void) {
    float x[JOINTS], y[JOINTS];
    const uint32_t bone_color=PVR_PACK_COLOR(0.20f,0.85f,1.0f,1.0f);
    const uint32_t joint_color=PVR_PACK_COLOR(1.0f,0.72f,0.22f,1.0f);
    for(unsigned i=0;i<JOINTS;++i) {
        x[i]=320.0f+pose[i*3]*112.0f;
        y[i]=222.0f+pose[i*3+1]*104.0f;
    }
    pvr_poly_cxt_t cxt; pvr_poly_hdr_t poly;
    pvr_poly_cxt_col(&cxt,PVR_LIST_TR_POLY);
    pvr_poly_compile(&poly,&cxt); pvr_prim(&poly,sizeof(poly));
    for(unsigned i=0;i<sizeof(links)/sizeof(links[0]);++i)
        draw_quad(x[links[i][0]],y[links[i][0]],x[links[i][1]],y[links[i][1]],7.0f,bone_color);
    for(unsigned i=0;i<JOINTS;++i)
        draw_quad(x[i]-4,y[i],x[i]+4,y[i],9.0f,joint_color);
    draw_quad(x[0]-10,y[0]-10,x[0]+10,y[0]+10,8.0f,joint_color);
}

static void draw_screen(void) {
    char line[80];
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    draw_pose();
    draw_text(24,20,"VIRTUA FIGHTER 3 - NAOMI");
    draw_text(24,52,"ORIGINAL VF3 MOTION EVALUATOR");
    snprintf(line,sizeof(line),"EVALUATOR: %s",evaluator_ok?"RUNNING":"ERROR");
    draw_text(24,84,line);
    snprintf(line,sizeof(line),"FRAME %u  PHASE %u/61",frame_no,(frame_no%59)+2);
    draw_text(24,116,line);
    draw_text(24,420,"SYNTHETIC MOTION FIXTURE - NOT GAMEPLAY");
    pvr_list_finish();
    pvr_scene_finish();
}

int main(void) {
    build_fixture();
    pvr_init_defaults();
    pvr_set_bg_color(0.015f,0.025f,0.055f);
    font_texture=pvr_mem_malloc(256*256*2);
    if(!font_texture) for(;;) thd_sleep(1000);
    font_init();
    evaluator_ok=1;
    for(;;) {
        float *out=pose;
        int32_t phase=(int32_t)(((frame_no%59)+2)*256);
        if(vf3_mt_eval_frame(&task,phase,&out)!=0 || out!=pose+63)
            evaluator_ok=0;
        draw_screen();
        ++frame_no;
        thd_sleep(16);
    }
    return 0;
}
