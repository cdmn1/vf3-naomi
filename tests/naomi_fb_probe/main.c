#include <kos.h>
#include <dc/pvr.h>
#include <stdint.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

#define PVR_FB_CFG_1 0xa05f8044u
#define PVR_FB_ADDR  0xa05f8050u
#define PVR_FB_SIZE  0xa05f805cu
#define PVR_IL_CFG   0xa05f80d0u
#define PVR_VRAM     0xa5000000u

static void direct_framebuffer_test(void) {
    uint32_t cfg = *(volatile uint32_t *)(uintptr_t)PVR_FB_CFG_1;
    uint32_t base = *(volatile uint32_t *)(uintptr_t)PVR_FB_ADDR;
    uint32_t size = *(volatile uint32_t *)(uintptr_t)PVR_FB_SIZE;
    uint32_t il = *(volatile uint32_t *)(uintptr_t)PVR_IL_CFG;
    uint32_t mode = (cfg >> 2) & 3u;
    uint32_t width = ((size & 0x3ffu) + 1u) * 2u;
    uint32_t height = ((size >> 10) & 0x3ffu) + 1u;
    if(il & 0x10u) height *= 2u;

    if(mode > 1u || width < 160u || height < 120u) {
        for(;;) thd_sleep(1000);
    }

    volatile uint16_t *fb = (volatile uint16_t *)(uintptr_t)
        (PVR_VRAM | (base & 0x007fffffu));
    uint16_t background = mode == 0u ? 0x5c00u : 0xb800u;
    uint16_t white = 0xffffu;
    uint16_t green = 0x07e0u;

    for(uint32_t y = 0; y < height; ++y)
        for(uint32_t x = 0; x < width; ++x)
            fb[y * width + x] = background;

    for(uint32_t y = 12; y < 68; ++y)
        for(uint32_t x = 8; x < 104; ++x)
            fb[y * width + x] = white;

    for(uint32_t y = 84; y < 140; ++y)
        for(uint32_t x = 8; x < 104; ++x)
            fb[y * width + x] = green;

    for(;;) thd_sleep(1000);
}

int main(void) {
    pvr_init_defaults();
    direct_framebuffer_test();
    return 0;
}
