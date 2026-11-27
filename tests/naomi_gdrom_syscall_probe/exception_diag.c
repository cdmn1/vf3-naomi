#include <stdint.h>
#define RT __attribute__((section(".vf3_resident_text"), noinline, used))
#define RD __attribute__((section(".vf3_resident_data"), used))
#define RO __attribute__((section(".vf3_resident_rodata"), used))
#define FBREG 0xa05f8050u
#define VRAM  0xa5000000u
#define VEC   0xac00f500u

typedef struct { uint32_t spc, ssr, sgr, expevt, tea, sr; } vf3_exception_info_t;
volatile vf3_exception_info_t vf3_exception_info RD;
static volatile uint32_t w RD, h RD, pm RD, active RD, calls RD;
static volatile uint32_t op RD, arg0 RD, arg1 RD, arg2 RD, cmd RD, fad RD, sectors RD, dest RD;
extern void vf3_exception_entry(void) __asm__("_vf3_exception_entry");

static const uint8_t font[36][5] RO = {
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
 {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
 {0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
 {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},
 {0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},
 {0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
 {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},
 {0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},
 {0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},
 {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
 {0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},
 {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
 {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
 {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
 {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
 {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
 {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e}
};
static const char digits[] RO = "0123456789ABCDEF";
static const char t_handoff[] RO = "VF3 HANDOFF";
static const char t_ready[] RO = "EXCEPTION MONITOR READY";
static const char t_wait[] RO = "WAITING FOR RETAIL CODE";
static const char t_entry[] RO = "ENTRY";
static const char t_bridge[] RO = "GDROM BRIDGE ACTIVE";
static const char t_syscall[] RO = "RETAIL GDROM CALL";
static const char t_request[] RO = "REQUEST DETAILS";
static const char t_exception[] RO = "SH4 EXCEPTION";
static const char t_stopped[] RO = "CPU STOPPED IN RETAIL STARTUP";
static const char l_op[] RO = "OP";
static const char l_arg[] RO = "ARG";
static const char l_ptr[] RO = "PTR";
static const char l_calls[] RO = "CALLS";
static const char l_cmd[] RO = "CMD";
static const char l_fad[] RO = "FAD";
static const char l_sectors[] RO = "SECTORS";
static const char l_dest[] RO = "DEST";
static const char l_expevt[] RO = "EXPEVT";
static const char l_spc[] RO = "SPC";
static const char l_ssr[] RO = "SSR";
static const char l_tea[] RO = "TEA";
static const char l_sgr[] RO = "SGR";
static const char l_sr[] RO = "SR";

static RT int gi(char c) {
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= '0' && c <= '9') return 26 + c - '0';
    return -1;
}
static RT uint16_t rgb(uint8_t r,uint8_t g,uint8_t b) {
    if(pm == 0u) return (uint16_t)(((r>>3)<<10)|((g>>3)<<5)|(b>>3));
    return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
static RT volatile uint16_t *fbptr(void) {
    uint32_t base=*(volatile uint32_t *)(uintptr_t)FBREG;
    return (volatile uint16_t *)(uintptr_t)(VRAM|(base&0x007fffffu));
}
static RT void clear(uint16_t c) {
    if(pm>1u || !w || !h) return;
    volatile uint16_t *p=fbptr();
    for(uint32_t i=0;i<w*h;i++) p[i]=c;
}
static RT void pixel(uint32_t x,uint32_t y,uint16_t c) {
    if(x<w && y<h && pm<=1u) fbptr()[y*w+x]=c;
}
static RT void text(uint32_t x,uint32_t y,const char *s,uint32_t scale,uint16_t c) {
    while(*s) {
        int n=gi(*s++);
        if(n<0) { x+=6u*scale; continue; }
        for(uint32_t col=0;col<5;col++) {
            uint8_t bits=font[n][col];
            for(uint32_t row=0;row<7;row++) if(bits&(1u<<row))
                for(uint32_t sy=0;sy<scale;sy++)
                    for(uint32_t sx=0;sx<scale;sx++)
                        pixel(x+col*scale+sx,y+row*scale+sy,c);
        }
        x+=6u*scale;
    }
}
static RT void hexval(uint32_t x,uint32_t y,uint32_t v,uint32_t scale,uint16_t c) {
    for(uint32_t i=0;i<8;i++) {
        int n=gi(digits[(v>>((7u-i)*4u))&15u]);
        for(uint32_t col=0;col<5;col++) {
            uint8_t bits=font[n][col];
            for(uint32_t row=0;row<7;row++) if(bits&(1u<<row))
                for(uint32_t sy=0;sy<scale;sy++)
                    for(uint32_t sx=0;sx<scale;sx++)
                        pixel(x+(i*6u+col)*scale+sx,y+row*scale+sy,c);
        }
    }
}
static RT void line(uint32_t y,const char *label,uint32_t v,uint16_t c) {
    text(24,y,label,2,c); hexval(190,y,v,2,c);
}
static RT void show_handoff(void) {
    clear(rgb(4,12,38));
    text(24,24,t_handoff,3,rgb(255,255,255));
    text(24,78,t_ready,2,rgb(80,255,160));
    text(24,116,t_wait,2,rgb(255,255,255));
    text(24,170,t_entry,2,rgb(255,220,80));
    hexval(110,170,0x8c010000u,2,rgb(255,220,80));
    text(24,220,t_bridge,2,rgb(120,200,255));
}
static RT void show_syscall(void) {
    clear(rgb(3,28,12));
    text(24,20,t_syscall,3,rgb(255,255,255));
    line(86,l_op,op,rgb(120,255,160));
    line(126,l_arg,arg0,rgb(255,255,255));
    line(166,l_ptr,arg1,rgb(255,255,255));
    line(206,l_calls,calls,rgb(255,220,80));
    text(24,270,t_request,2,rgb(120,200,255));
    line(310,l_cmd,cmd,rgb(255,255,255));
    line(350,l_fad,fad,rgb(255,255,255));
    line(390,l_sectors,sectors,rgb(255,255,255));
    line(430,l_dest,dest,rgb(255,255,255));
}
static RT void show_exception(void) {
    clear(rgb(45,0,0));
    text(24,16,t_exception,3,rgb(255,255,255));
    line(78,l_expevt,vf3_exception_info.expevt,rgb(255,220,80));
    line(118,l_spc,vf3_exception_info.spc,rgb(255,255,255));
    line(158,l_ssr,vf3_exception_info.ssr,rgb(255,255,255));
    line(198,l_tea,vf3_exception_info.tea,rgb(255,255,255));
    line(238,l_sgr,vf3_exception_info.sgr,rgb(255,255,255));
    line(278,l_sr,vf3_exception_info.sr,rgb(255,255,255));
    text(24,340,t_stopped,2,rgb(255,180,180));
}
RT uint32_t vf3_diag_prepare(uint32_t width,uint32_t height,uint32_t pixel_mode) {
    volatile uint16_t *v=(volatile uint16_t *)(uintptr_t)VEC;
    volatile uint32_t *target=(volatile uint32_t *)(uintptr_t)(VEC+8u);
    w=width; h=height; pm=pixel_mode; active=0; calls=0;
    if(width<320u || height<240u || pixel_mode>1u) return 0u;
    /* mov.l @(1,pc),r0 ; jmp @r0 ; nop ; padding ; absolute handler */
    v[0]=0xd001u; v[1]=0x402bu; v[2]=0x0009u; v[3]=0x0009u;
    *target=(uint32_t)(uintptr_t)vf3_exception_entry;
    if(v[0]!=0xd001u || v[1]!=0x402bu ||
       *target!=(uint32_t)(uintptr_t)vf3_exception_entry) return 0u;
    return 1u;
}
RT void vf3_diag_begin(void) { active=1u; show_handoff(); }
RT void vf3_diag_note_operation(uint32_t operation,uint32_t a,uint32_t b,uint32_t c) {
    if(!active || operation==1u || operation==6u || operation==7u ||
       operation==12u || operation==13u) return;
    calls++; op=operation; arg0=a; arg1=b; arg2=c; show_syscall();
}
RT void vf3_diag_note_request(uint32_t command,const uint32_t params[4]) {
    if(!active) return;
    cmd=command;
    if(params) { fad=params[0]; sectors=params[1]; dest=params[2]; }
    show_syscall();
}
RT void vf3_diag_exception_display(void) {
    show_exception();
    for(;;) __asm__ volatile("nop");
}
