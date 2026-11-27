#include <kos.h>
#include <dc/minifont.h>
#include <dc/pvr.h>
#include <stdint.h>
#include <stdio.h>
#include "gdrom_syscall.h"
#include "vf3_asset_map.h"
#include "ip_template_data.h"

#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define NAOMI_ROM_OFFSETH 0x005f7000u
#define NAOMI_ROM_OFFSETL 0x005f7004u
#define NAOMI_ROM_DATA 0x005f7008u
#define FONT_W 12
#define FONT_H 24
#define WORDS 1024u
#define TEST_FAD VF3_ASSET_TEST_FAD
#define TEST_OFFSET VF3_ASSET_TEST_OFFSET
#if VF3_ASSET_MAP_COUNT > 0
/* 1ST_READ.BIN is 576 sectors; the 1 MiB DMA cap forces two chunks. */
#define DMA_TEST_SECTORS 576u
#else
#define DMA_TEST_SECTORS 1u
#endif

KOS_INIT_FLAGS(INIT_DEFAULT);

static uint16_t pio_words[WORDS];
static uint8_t dma_buffer[DMA_TEST_SECTORS * VF3_GD_SECTOR_BYTES] __attribute__((aligned(32)));
static uint8_t pio_bridge_buffer[WORDS * 2u] __attribute__((aligned(32)));
static pvr_ptr_t font_texture;
static int dma_pass, pio_bridge_pass, direct_match, bridge_match, status_pass;
static uint32_t old_service, dma_status, dma_bytes, dma_request, pio_request;
static int retail_load_pass, retail_sha_pass, ip_boot_area_pass, exception_monitor_pass;
#define RETAIL_STAGE_P1 0x8d100000u
#define RETAIL_STAGE_P2 0xad100000u
#define RETAIL_EXEC_BYTES 1179648u
static uint32_t rr(uint32_t x,unsigned n){return (x>>n)|(x<<(32u-n));}
static void sha256(const uint8_t *p,uint32_t n,uint8_t out[32]){
 static const uint32_t K[64]={
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t H[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 uint32_t bits=n*8u,total=(n+9u+63u)&~63u;
 for(uint32_t off=0;off<total;off+=64u){
  uint32_t w[64],a,b,c,d,e,f,g,h; uint8_t q[64];
  for(uint32_t j=0;j<64;j++){uint32_t x=off+j;
   q[j]=x<n?p[x]:(x==n?0x80u:(x>=total-4u?(uint8_t)(bits>>((total-1u-x)*8u)):0u));}
  for(uint32_t j=0;j<16;j++)w[j]=((uint32_t)q[4*j]<<24)|((uint32_t)q[4*j+1]<<16)|((uint32_t)q[4*j+2]<<8)|q[4*j+3];
  for(uint32_t j=16;j<64;j++){uint32_t x=rr(w[j-15],7)^rr(w[j-15],18)^(w[j-15]>>3),y=rr(w[j-2],17)^rr(w[j-2],19)^(w[j-2]>>10);w[j]=w[j-16]+x+w[j-7]+y;}
  a=H[0];b=H[1];c=H[2];d=H[3];e=H[4];f=H[5];g=H[6];h=H[7];
  for(uint32_t j=0;j<64;j++){uint32_t s1=rr(e,6)^rr(e,11)^rr(e,25),ch=(e&f)^(~e&g),t1=h+s1+ch+K[j]+w[j],s0=rr(a,2)^rr(a,13)^rr(a,22),maj=(a&b)^(a&c)^(b&c),t2=s0+maj;h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
  H[0]+=a;H[1]+=b;H[2]+=c;H[3]+=d;H[4]+=e;H[5]+=f;H[6]+=g;H[7]+=h;
 }
 for(uint32_t i=0;i<8;i++){out[4*i]=H[i]>>24;out[4*i+1]=H[i]>>16;out[4*i+2]=H[i]>>8;out[4*i+3]=H[i];}
}
static int install_ip_boot_area(void){
 const uint8_t *src=vf3_ip_template;
 volatile uint8_t *dst=(volatile uint8_t *)(uintptr_t)0xac008000u;
 if(sizeof vf3_ip_template!=0x8000u)return 0;
 for(uint32_t i=0;i<0x8000u;i++)dst[i]=src[i];
 for(uint32_t i=0;i<0x8000u;i++)if(dst[i]!=src[i])return 0;
 return 1;
}
static int retail_hash_matches(const uint8_t *data){
 static const uint8_t expected[32]={0x37,0xf3,0x8d,0xd9,0xa4,0x7b,0x78,0x5f,0x52,0x61,0x57,0xc4,0xa1,0x67,0x4c,0xa7,0x53,0x22,0xf0,0xde,0x13,0x42,0xf6,0x4b,0xbf,0x33,0xc8,0x8d,0x12,0x8e,0x4b,0x32};
 uint8_t got[32];sha256(data,RETAIL_EXEC_BYTES,got);for(unsigned i=0;i<32;i++)if(got[i]!=expected[i])return 0;return 1;
}
static void run_retail_load_check(void){
#if VF3_ASSET_MAP_COUNT > 0
 uint32_t p[4]={TEST_FAD,576u,RETAIL_STAGE_P1,0u},left=0xffffffffu,ext[4]={0,0,0,0},check=0;unsigned wait=0;
 uint32_t id=vf3_gd_req_cmd(VF3_GD_CMD_DMAREAD,p);if(!id)return;vf3_gd_exec_server();
 do{check=vf3_gd_check_dma_trans(id,&left);if(check)break;thd_sleep(1);}while(++wait<5000u);
 uint32_t status=vf3_gd_get_cmd_stat(id,ext);
 retail_load_pass=(check==1u&&status==VF3_GDC_COMPLETE&&left==0u&&ext[2]==RETAIL_EXEC_BYTES);
 if(retail_load_pass)retail_sha_pass=retail_hash_matches((const uint8_t *)(uintptr_t)RETAIL_STAGE_P2);
#endif
}


static void read_cart_pio(void)
{
    /* Naomi cartridge PIO offsets are byte addresses; each data read
       advances the internal address by two bytes when auto-increment is set. */
    REG16(NAOMI_ROM_OFFSETH) = (uint16_t)(0x8000u | ((TEST_OFFSET >> 16) & 0x7fffu));
    REG16(NAOMI_ROM_OFFSETL) = (uint16_t)TEST_OFFSET;
    for (unsigned i = 0; i < WORDS; ++i)
        pio_words[i] = REG16(NAOMI_ROM_DATA);
}

static int compare_uncached(const uint16_t *expected, const uint8_t *buffer)
{
    const volatile uint16_t *actual =
        (const volatile uint16_t *)((uintptr_t)buffer | 0x20000000u);
    for (unsigned i = 0; i < WORDS; ++i)
        if (expected[i] != actual[i])
            return 0;
    return 1;
}

static void run_dma_syscall(void)
{
    uint32_t params[4] = { TEST_FAD, DMA_TEST_SECTORS, (uint32_t)(uintptr_t)dma_buffer, 0u };
    uint32_t remaining = 0xffffffffu, ext[4] = {0,0,0,0};
    uint32_t check = 0u;
    unsigned wait = 0;
    dma_request = vf3_gd_req_cmd(VF3_GD_CMD_DMAREAD, params);
    if (!dma_request) return;
    vf3_gd_req_dma_trans(dma_request, &remaining);
    vf3_gd_exec_server();
    do {
        check = vf3_gd_check_dma_trans(dma_request, &remaining);
        if (check != 0u) break;
        thd_sleep(1);
    } while (++wait < 3000u);
    dma_status = vf3_gd_get_cmd_stat(dma_request, ext);
    dma_bytes = ext[2];
    dma_pass = check == 1u && dma_status == VF3_GDC_COMPLETE &&
               remaining == 0u && dma_bytes == DMA_TEST_SECTORS * VF3_GD_SECTOR_BYTES;
    status_pass = dma_status == VF3_GDC_COMPLETE;
}

static void run_pio_to_dma_bridge(void)
{
    uint32_t p2_dest = 0xa0000000u |
        ((uint32_t)(uintptr_t)pio_bridge_buffer & 0x1fffffffu);
    uint32_t params[4] = { TEST_FAD, 1u, p2_dest, 0u };
    uint32_t remaining = 0xffffffffu, ext[4] = {0,0,0,0};
    uint32_t check = 0u, status;
    unsigned wait = 0;
    pio_request = vf3_gd_req_cmd(VF3_GD_CMD_PIOREAD, params);
    if (!pio_request) return;
    vf3_gd_req_pio_trans(pio_request, &remaining);
    vf3_gd_exec_server();
    do {
        check = vf3_gd_check_pio_trans(pio_request, &remaining);
        if (check != 0u) break;
        thd_sleep(1);
    } while (++wait < 3000u);
    status = vf3_gd_get_cmd_stat(pio_request, ext);
    pio_bridge_pass = check == 1u && status == VF3_GDC_COMPLETE &&
                      remaining == 0u && ext[2] == VF3_GD_SECTOR_BYTES;
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
    const int ix = (c % 16) * 16, iy = (c / 16) * 24;
    const float u1 = ix / 256.0f, v1 = iy / 256.0f;
    const float u2 = (ix + FONT_W) / 256.0f, v2 = (iy + FONT_H) / 256.0f;
    if (c <= ' ' || c >= 127) return;
    v.flags=PVR_CMD_VERTEX; v.x=x; v.y=y+FONT_H; v.z=1.0f;
    v.u=u1; v.v=v2; v.argb=PVR_PACK_COLOR(1,1,1,1); v.oargb=0;
    pvr_prim(&v,sizeof v);
    v.x=x; v.y=y; v.u=u1; v.v=v1; pvr_prim(&v,sizeof v);
    v.x=x+FONT_W; v.y=y+FONT_H; v.u=u2; v.v=v2; pvr_prim(&v,sizeof v);
    v.flags=PVR_CMD_VERTEX_EOL; v.x=x+FONT_W; v.y=y; v.u=u2; v.v=v1;
    pvr_prim(&v,sizeof v);
}

static void draw_text(float x, float y, const char *s)
{
    pvr_poly_cxt_t cxt; pvr_poly_hdr_t poly;
    if (!s || !s[0]) return;
    pvr_poly_cxt_txr(&cxt,PVR_LIST_TR_POLY,
        PVR_TXRFMT_ARGB1555|PVR_TXRFMT_NONTWIDDLED,
        256,256,font_texture,PVR_FILTER_NONE);
    pvr_poly_compile(&poly,&cxt); pvr_prim(&poly,sizeof poly);
    for (size_t i=0;s[i];++i) { draw_char(x,y,(unsigned char)s[i]); x+=FONT_W; }
}

static void draw_screen(void)
{
    char line[96];
    char resident_line[96];
    pvr_wait_ready(); pvr_scene_begin(); pvr_list_begin(PVR_LIST_TR_POLY);
    draw_text(32,32,"VF3 NAOMI GD-ROM SYSCALL BRIDGE");
    draw_text(32,76,"BIOS SERVICE VECTOR: INSTALLED");
    draw_text(32,112,dma_pass?"DMAREAD / HOLLY DMA: PASS":"DMAREAD / HOLLY DMA: FAIL");
    draw_text(32,148,pio_bridge_pass?"PIOREAD TRANSLATED TO DMA: PASS":"PIOREAD TRANSLATED TO DMA: FAIL");
    draw_text(32,184,direct_match?"DIRECT PIO / DMA DATA MATCH: PASS":"DIRECT PIO / DMA DATA MATCH: FAIL");
    draw_text(32,220,bridge_match?"PIOREAD / DMA DATA MATCH: PASS":"PIOREAD / DMA DATA MATCH: FAIL");
    draw_text(32,256,status_pass?"GET_CMD_STAT COMPLETION: PASS":"GET_CMD_STAT COMPLETION: FAIL");
    snprintf(line,sizeof line,"DMA STATUS %08X  BYTES %u",(unsigned)dma_status,(unsigned)dma_bytes);
    draw_text(32,292,line);
    snprintf(line,sizeof line,"OLD SERVICE VECTOR %08X",(unsigned)old_service);
    draw_text(32,328,line);
    snprintf(line,sizeof line,"TEST FAD %u  CART OFFSET %08X",(unsigned)TEST_FAD,(unsigned)TEST_OFFSET);
    draw_text(32,364,line);
    draw_text(32,388,exception_monitor_pass?"EXCEPTION MONITOR: READY":"EXCEPTION MONITOR: NOT READY");
    snprintf(resident_line, sizeof resident_line, "IP.BIN BOOT AREA: %s", ip_boot_area_pass?"PASS":"FAIL");
    draw_text(32,412,resident_line);
    draw_text(32,436,retail_load_pass?"RETAIL 1ST_READ LOAD: PASS":"RETAIL 1ST_READ LOAD: NOT VERIFIED");
    draw_text(32,460,retail_sha_pass?"1ST_READ SHA256: MATCH":"1ST_READ SHA256: NOT MATCHED");
    pvr_list_finish(); pvr_scene_finish();
}

int main(void)
{
    read_cart_pio();
    old_service = vf3_gd_install();
    run_dma_syscall();
    /* Preserve the multi-chunk DMA result before the PIOREAD request replaces
       the bridge state with its own one-sector transfer. */
    dma_status = vf3_gd_last_dma_status();
    dma_bytes = vf3_gd_last_bytes();
    direct_match = compare_uncached(pio_words,dma_buffer);
    run_pio_to_dma_bridge();
    bridge_match = compare_uncached(pio_words,pio_bridge_buffer);
    run_retail_load_check();
    if (retail_load_pass && retail_sha_pass)
        ip_boot_area_pass = install_ip_boot_area();

    pvr_init_defaults();
    pvr_set_bg_color(0.02f,0.06f,0.12f);
    font_texture = pvr_mem_malloc(256u*256u*2u);
    if (!font_texture) for (;;) thd_sleep(1000);
    font_init();
    if (retail_load_pass && retail_sha_pass && ip_boot_area_pass)
        exception_monitor_pass = vf3_gd_diag_prepare((uint32_t)vid_mode->width,
            (uint32_t)vid_mode->height, (uint32_t)vid_mode->pm) == 1u;
    if (retail_load_pass && retail_sha_pass && ip_boot_area_pass && exception_monitor_pass) {
        /* Let the user see the successful retail hash check before replacing
           the low KOS image with the Dreamcast retail executable. */
        for (unsigned i = 0; i < 180u; ++i) { draw_screen(); thd_sleep(16); }
        vf3_gd_chainload();
    }
    for (;;) { draw_screen(); thd_sleep(16); }
    return 0;
}
