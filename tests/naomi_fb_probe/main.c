#include <kos.h>
#include <dc/pvr.h>
#include <stdint.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

static void rect(float x0, float y0, float x1, float y1, uint32_t color) {
    pvr_vertex_t v;
    v.flags=PVR_CMD_VERTEX; v.x=x0; v.y=y1; v.z=1.0f;
    v.u=0.0f; v.v=0.0f; v.argb=color; v.oargb=0;
    pvr_prim(&v,sizeof(v));
    v.x=x0; v.y=y0; pvr_prim(&v,sizeof(v));
    v.x=x1; v.y=y1; pvr_prim(&v,sizeof(v));
    v.flags=PVR_CMD_VERTEX_EOL; v.x=x1; v.y=y0; pvr_prim(&v,sizeof(v));
}

static void frame(void) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_poly_cxt_col(&cxt,PVR_LIST_OP_POLY);
    pvr_poly_compile(&hdr,&cxt);
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&hdr,sizeof(hdr));
    rect(24,24,296,88,0xffff0000u);
    rect(24,104,296,168,0xff00ff00u);
    rect(24,184,296,248,0xff0000ffu);
    rect(320,24,592,88,0xffffffffu);
    rect(320,104,592,168,0xffffff00u);
    rect(320,184,592,248,0xffff00ffu);
    pvr_list_finish();
    pvr_scene_finish();
}

int main(void) {
    pvr_init_defaults();
    pvr_set_bg_color(0.02f,0.02f,0.02f);
    for (;;) {
        frame();
        thd_sleep(16);
    }
    return 0;
}
