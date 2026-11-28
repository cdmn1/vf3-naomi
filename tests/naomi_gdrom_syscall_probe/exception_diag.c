#include <stdint.h>
#define RT __attribute__((section(".vf3_resident_text"), noinline, used))
#define RD __attribute__((section(".vf3_resident_data"), used))
#define RO __attribute__((section(".vf3_resident_rodata"), used))
#define FBREG 0xa05f8050u
#define FBCFG 0xa05f8044u
#define FBSIZE 0xa05f805cu
#define ILCFG 0xa05f80d0u
#define VRAM  0xa5000000u
#define VEC_GENERAL 0xac00f500u
#define VEC_TLB     0xac00f800u

typedef struct { uint32_t spc, ssr, sgr, expevt, tea, sr; } vf3_exception_info_t;
volatile vf3_exception_info_t vf3_exception_info RD;
static volatile uint32_t w RD, h RD, pm RD, active RD, calls RD;
static volatile uint32_t screen_w RD, screen_h RD, screen_pm RD;
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
static const char t_chainload[] RO = "CHAINLOAD PROGRESS";
static const char t_stage1[] RO = "P1 ASSEMBLY ENTRY";
static const char t_stage2[] RO = "P2 ENTRY CONFIRMED";
static const char t_stage3[] RO = "CACHE INVALIDATION HALF";
static const char t_stage4[] RO = "CACHE INVALIDATION DONE";
static const char t_stage5[] RO = "RETAIL IMAGE COPY DONE";
static const char t_stage6[] RO = "CCR UPDATE DONE";
static const char t_stage7[] RO = "TRAPA INSTRUCTION NEXT";
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
static RT int refresh_screen_mode(void) {
    uint32_t cfg=*(volatile uint32_t *)(uintptr_t)FBCFG;
    uint32_t size=*(volatile uint32_t *)(uintptr_t)FBSIZE;
    uint32_t il=*(volatile uint32_t *)(uintptr_t)ILCFG;
    uint32_t mode=(cfg>>2)&3u;
    /* This monitor intentionally supports only the 16-bit framebuffer modes. */
    uint32_t width=((size&0x3ffu)+1u)*2u;
    uint32_t height=((size>>10)&0x3ffu)+1u;
    if(il&0x10u) height*=2u;
    if(width<160u || height<120u || mode>1u) return 0;
    screen_w=width; screen_h=height; screen_pm=mode;
    return 1;
}
static RT uint16_t rgb(uint8_t r,uint8_t g,uint8_t b) {
    if(screen_pm == 0u) return (uint16_t)(((r>>3)<<10)|((g>>3)<<5)|(b>>3));
    return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
static RT volatile uint16_t *fbptr(void) {
    uint32_t base=*(volatile uint32_t *)(uintptr_t)FBREG;
    return (volatile uint16_t *)(uintptr_t)(VRAM|(base&0x007fffffu));
}
static RT void clear(uint16_t c) {
    if(screen_pm>1u || !screen_w || !screen_h) return;
    volatile uint16_t *p=fbptr();
    for(uint32_t i=0;i<screen_w*screen_h;i++) p[i]=c;
}
static RT void pixel(uint32_t x,uint32_t y,uint16_t c) {
    if(x<screen_w && y<screen_h && screen_pm<=1u) fbptr()[y*screen_w+x]=c;
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
    text(8,y,label,2,c); hexval(104,y,v,2,c);
}
static RT void show_handoff(void) {
    if(!refresh_screen_mode()) return;
    clear(rgb(4,12,38));
    text(8,8,t_handoff,2,rgb(255,255,255));
    text(8,40,t_ready,1,rgb(80,255,160));
    text(8,58,t_wait,1,rgb(255,255,255));
    text(8,90,t_entry,2,rgb(255,220,80));
    hexval(80,90,0x8c010000u,2,rgb(255,220,80));
    text(8,124,t_bridge,1,rgb(120,200,255));
}
static RT void show_syscall(void) {
    if(!refresh_screen_mode()) return;
    clear(rgb(3,28,12));
    text(8,8,t_syscall,2,rgb(255,255,255));
    line(36,l_op,op,rgb(120,255,160));
    line(58,l_arg,arg0,rgb(255,255,255));
    line(80,l_ptr,arg1,rgb(255,255,255));
    line(102,l_calls,calls,rgb(255,220,80));
    text(8,128,t_request,2,rgb(120,200,255));
    line(150,l_cmd,cmd,rgb(255,255,255));
    line(172,l_fad,fad,rgb(255,255,255));
    line(194,l_sectors,sectors,rgb(255,255,255));
    line(216,l_dest,dest,rgb(255,255,255));
}

/* Exception screen renderer: avoids resident font/rodata lookups. */
static RT uint8_t exception_hex_col(uint32_t n,uint32_t c) {
 switch(n) {
 case 0: switch(c) {
  case 0: return 0x3eu;
  case 1: return 0x41u;
  case 2: return 0x41u;
  case 3: return 0x41u;
  case 4: return 0x3eu;
  default: return 0u;
 }
 case 1: switch(c) {
  case 0: return 0x0u;
  case 1: return 0x42u;
  case 2: return 0x7fu;
  case 3: return 0x40u;
  case 4: return 0x0u;
  default: return 0u;
 }
 case 2: switch(c) {
  case 0: return 0x42u;
  case 1: return 0x61u;
  case 2: return 0x51u;
  case 3: return 0x49u;
  case 4: return 0x46u;
  default: return 0u;
 }
 case 3: switch(c) {
  case 0: return 0x21u;
  case 1: return 0x41u;
  case 2: return 0x45u;
  case 3: return 0x4bu;
  case 4: return 0x31u;
  default: return 0u;
 }
 case 4: switch(c) {
  case 0: return 0x18u;
  case 1: return 0x14u;
  case 2: return 0x12u;
  case 3: return 0x7fu;
  case 4: return 0x10u;
  default: return 0u;
 }
 case 5: switch(c) {
  case 0: return 0x27u;
  case 1: return 0x45u;
  case 2: return 0x45u;
  case 3: return 0x45u;
  case 4: return 0x39u;
  default: return 0u;
 }
 case 6: switch(c) {
  case 0: return 0x3cu;
  case 1: return 0x4au;
  case 2: return 0x49u;
  case 3: return 0x49u;
  case 4: return 0x30u;
  default: return 0u;
 }
 case 7: switch(c) {
  case 0: return 0x1u;
  case 1: return 0x71u;
  case 2: return 0x9u;
  case 3: return 0x5u;
  case 4: return 0x3u;
  default: return 0u;
 }
 case 8: switch(c) {
  case 0: return 0x36u;
  case 1: return 0x49u;
  case 2: return 0x49u;
  case 3: return 0x49u;
  case 4: return 0x36u;
  default: return 0u;
 }
 case 9: switch(c) {
  case 0: return 0x6u;
  case 1: return 0x49u;
  case 2: return 0x49u;
  case 3: return 0x29u;
  case 4: return 0x1eu;
  default: return 0u;
 }
 case 10: switch(c) {
  case 0: return 0x7eu;
  case 1: return 0x11u;
  case 2: return 0x11u;
  case 3: return 0x11u;
  case 4: return 0x7eu;
  default: return 0u;
 }
 case 11: switch(c) {
  case 0: return 0x7fu;
  case 1: return 0x49u;
  case 2: return 0x49u;
  case 3: return 0x49u;
  case 4: return 0x36u;
  default: return 0u;
 }
 case 12: switch(c) {
  case 0: return 0x3eu;
  case 1: return 0x41u;
  case 2: return 0x41u;
  case 3: return 0x41u;
  case 4: return 0x22u;
  default: return 0u;
 }
 case 13: switch(c) {
  case 0: return 0x7fu;
  case 1: return 0x41u;
  case 2: return 0x41u;
  case 3: return 0x22u;
  case 4: return 0x1cu;
  default: return 0u;
 }
 case 14: switch(c) {
  case 0: return 0x7fu;
  case 1: return 0x49u;
  case 2: return 0x49u;
  case 3: return 0x49u;
  case 4: return 0x41u;
  default: return 0u;
 }
 case 15: switch(c) {
  case 0: return 0x7fu;
  case 1: return 0x9u;
  case 2: return 0x9u;
  case 3: return 0x9u;
  case 4: return 0x1u;
  default: return 0u;
 }
 default: return 0u;
 }
}
static RT void exception_draw_nibble(volatile uint16_t *fb,uint32_t x,uint32_t y,
                                     uint32_t n,uint16_t color) {
    for(uint32_t col=0;col<5;col++) {
        uint8_t bits=exception_hex_col(n,col);
        for(uint32_t row=0;row<7;row++) if(bits&(1u<<row))
            for(uint32_t sy=0;sy<2;sy++)
                for(uint32_t sx=0;sx<2;sx++)
                    fb[(y+row*2u+sy)*screen_w+x+col*2u+sx]=color;
    }
}
static RT void exception_draw_value(volatile uint16_t *fb,uint32_t x,uint32_t y,
                                    uint32_t value,uint16_t color) {
    for(uint32_t i=0;i<8;i++)
        exception_draw_nibble(fb,x+i*14u,y,(value>>((7u-i)*4u))&15u,color);
}

static RT void show_exception(void) {
    if(!refresh_screen_mode()) return;
    clear(rgb(45,0,0));

    /*
     * Minimal post-clear framebuffer diagnostic. Deliberately avoid:
     * - exception-info reads
     * - local arrays or stack-derived values
     * - font/rodata accesses
     * - helper calls after clear
     *
     * If these fixed blocks appear, execution continued after clear and
     * direct framebuffer writes work. Register rendering can then be
     * reintroduced in separately verified steps.
     */
    volatile uint16_t *fb=fbptr();
    for(uint32_t y=12u;y<36u;y++) {
        for(uint32_t x=8u;x<56u;x++)
            fb[y*screen_w+x]=0xffffu;
    }
    for(uint32_t y=44u;y<68u;y++) {
        for(uint32_t x=8u;x<56u;x++)
            fb[y*screen_w+x]=0x07e0u;
    }
}

RT uint32_t vf3_diag_prepare(uint32_t width,uint32_t height,uint32_t pixel_mode) {
    volatile uint16_t *general=(volatile uint16_t *)(uintptr_t)VEC_GENERAL;
    volatile uint16_t *tlb=(volatile uint16_t *)(uintptr_t)VEC_TLB;
    volatile uint32_t *general_target=(volatile uint32_t *)(uintptr_t)(VEC_GENERAL+8u);
    volatile uint32_t *tlb_target=(volatile uint32_t *)(uintptr_t)(VEC_TLB+8u);
    w=width; h=height; pm=pixel_mode; active=0; calls=0;
    if(width<320u || height<240u || pixel_mode>1u) return 0u;
    /* General exceptions use VBR+0x100; TLB misses use VBR+0x400. */
    general[0]=0xd001u; general[1]=0x402bu;
    general[2]=0x0009u; general[3]=0x0009u;
    *general_target=(uint32_t)(uintptr_t)vf3_exception_entry;
    tlb[0]=0xd001u; tlb[1]=0x402bu;
    tlb[2]=0x0009u; tlb[3]=0x0009u;
    *tlb_target=(uint32_t)(uintptr_t)vf3_exception_entry;
    if(general[0]!=0xd001u || general[1]!=0x402bu ||
       *general_target!=(uint32_t)(uintptr_t)vf3_exception_entry ||
       tlb[0]!=0xd001u || tlb[1]!=0x402bu ||
       *tlb_target!=(uint32_t)(uintptr_t)vf3_exception_entry) return 0u;
    return 1u;
}
RT void vf3_diag_begin(uint32_t stage) {
    const char *label = t_stage1;
    if(stage==0u) { active=1u; show_handoff(); return; }
    if(!refresh_screen_mode()) return;
    if(stage==2u) label=t_stage2;
    else if(stage==3u) label=t_stage3;
    else if(stage==4u) label=t_stage4;
    else if(stage==5u) label=t_stage5;
    else if(stage==6u) label=t_stage6;
    else if(stage==7u) label=t_stage7;
    clear(rgb(3,12,42));
    text(8,8,t_chainload,2,rgb(255,255,255));
    text(8,44,label,1,rgb(120,255,160));
    text(8,78,t_entry,2,rgb(255,220,80));
    hexval(80,78,0x8c010000u,2,rgb(255,220,80));
    text(8,116,t_bridge,1,rgb(120,200,255));
    text(8,152,t_wait,1,rgb(255,255,255));
}

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
RT void vf3_diag_exception_draw_once(void) {
    show_exception();
}
RT void vf3_diag_exception_display(void) {
    show_exception();
    for(;;) __asm__ volatile("nop");
}
