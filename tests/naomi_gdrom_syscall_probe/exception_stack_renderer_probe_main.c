#include <kos.h>
#include <stdint.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint32_t vf3_gd_install(void);
extern uint32_t vf3_gd_diag_exception_stack_renderer(uint32_t stack_top);

#define PVR_FB_ADDR 0xa05f8050u
#define PVR_FB_SIZE 0xa05f805cu
#define PVR_FB_CFG  0xa05f8044u
#define PVR_IL_CFG  0xa05f80d0u
#define PVR_VRAM    0xa5000000u
#define PROBE_PASS  0x6ca7cafeu

static uint8_t renderer_stack[4096] __attribute__((aligned(16)));

static void show_result(uint32_t result)
{
    uint32_t cfg = *(volatile uint32_t *)(uintptr_t)PVR_FB_CFG;
    uint32_t base = *(volatile uint32_t *)(uintptr_t)PVR_FB_ADDR;
    uint32_t size = *(volatile uint32_t *)(uintptr_t)PVR_FB_SIZE;
    uint32_t il = *(volatile uint32_t *)(uintptr_t)PVR_IL_CFG;
    uint32_t mode = (cfg >> 2) & 3u;
    uint32_t width = ((size & 0x3ffu) + 1u) * 2u;
    uint32_t height = ((size >> 10) & 0x3ffu) + 1u;
    if (il & 0x10u) height *= 2u;
    if (mode > 1u || width < 160u || height < 120u)
        for (;;) thd_sleep(1000);

    volatile uint16_t *fb = (volatile uint16_t *)(uintptr_t)
        (PVR_VRAM | (base & 0x007fffffu));
    int pass = result == PROBE_PASS;
    uint16_t background = pass ? 0x001fu : (mode == 0u ? 0x7c00u : 0xf800u);
    uint16_t marker = pass ? 0x07e0u : 0xffe0u;
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            fb[y * width + x] = background;
    for (uint32_t y = 12; y < 68; ++y)
        for (uint32_t x = 8; x < 104; ++x)
            fb[y * width + x] = 0xffffu;
    for (uint32_t y = 84; y < 140; ++y)
        for (uint32_t x = 8; x < 104; ++x)
            fb[y * width + x] = marker;
    for (;;) thd_sleep(1000);
}

int main(void)
{
    pvr_init_defaults();
    if (vf3_gd_install() == 0xffffffffu)
        for (;;) thd_sleep(1000);
    uint32_t result = vf3_gd_diag_exception_stack_renderer(
        (uint32_t)(uintptr_t)(renderer_stack + sizeof(renderer_stack)));
    show_result(result);
    return 0;
}
