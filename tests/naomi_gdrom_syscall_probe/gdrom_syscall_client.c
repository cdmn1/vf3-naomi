#include <stddef.h>
#include <stdint.h>
#include <arch/cache.h>
#include "gdrom_syscall.h"
#include "resident_offsets.h"

extern const uint8_t resident_blob_start[] __asm__("_binary_resident_shim_bin_start");
extern const uint8_t resident_blob_end[] __asm__("_binary_resident_shim_bin_end");

typedef uint32_t (*resident_fn0)(void);
typedef void (*resident_fn_void)(void);
typedef void (*resident_fn1)(uint32_t);
typedef uint32_t (*resident_fn3)(uint32_t, uint32_t, uint32_t);

static int resident_loaded;

static int load_resident_image(void)
{
    const uint8_t *src = resident_blob_start;
    size_t size = (size_t)(resident_blob_end - resident_blob_start);
    volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)0xad000000u;
    if (resident_loaded)
        return 0;
    if (size != VF3_RESIDENT_IMAGE_SIZE)
        return -1;

    /* P2 is the uncached alias of the reserved 0x8Dxxxxxx RAM window. */
    for (size_t i = 0; i < size; ++i)
        dst[i] = src[i];

    /* Discard stale D-cache lines and invalidate I-cache for the copied code. */
    arch_dcache_inval_range(VF3_RESIDENT_IMAGE_BASE, size);
    arch_icache_inval_range(VF3_RESIDENT_IMAGE_BASE, size);
    resident_loaded = 1;
    return 0;
}

uint32_t vf3_gd_install(void)
{
    if (load_resident_image() != 0)
        return 0xffffffffu;
    return ((resident_fn0)(uintptr_t)VF3_RESIDENT_ADDR_GD_INSTALL)();
}

uint32_t vf3_gd_original_service(void)
{
    if (load_resident_image() != 0)
        return 0u;
    return ((resident_fn0)(uintptr_t)VF3_RESIDENT_ADDR_ORIGINAL_SERVICE)();
}

uint32_t vf3_gd_last_dma_status(void)
{
    if (load_resident_image() != 0)
        return 0xffffffffu;
    return ((resident_fn0)(uintptr_t)VF3_RESIDENT_ADDR_LAST_DMA_STATUS)();
}

uint32_t vf3_gd_last_bytes(void)
{
    if (load_resident_image() != 0)
        return 0u;
    return ((resident_fn0)(uintptr_t)VF3_RESIDENT_ADDR_LAST_BYTES)();
}

uint32_t vf3_gd_diag_prepare(uint32_t width, uint32_t height, uint32_t pixel_mode)
{
    if (load_resident_image() != 0)
        return 0u;
    return ((resident_fn3)(uintptr_t)VF3_RESIDENT_ADDR_DIAG_PREPARE)(width, height, pixel_mode);
}

void vf3_gd_diag_exception_entry(void)
{
    if (load_resident_image() == 0)
        ((resident_fn_void)(uintptr_t)VF3_RESIDENT_ADDR_EXCEPTION_ENTRY)();
    for (;;) { }
}

void vf3_gd_diag_exception_display(void)
{
    if (load_resident_image() == 0)
        ((resident_fn_void)(uintptr_t)VF3_RESIDENT_ADDR_DIAG_EXCEPTION_DISPLAY)();
    for (;;) { }
}

void vf3_gd_diag_exception_stack(uint32_t stack_top)
{
    if (load_resident_image() == 0)
        ((resident_fn1)(uintptr_t)VF3_RESIDENT_ADDR_EXCEPTION_STACK_PROBE)(stack_top);
    for (;;) { }
}

void vf3_gd_chainload(void)
{
    if (load_resident_image() == 0) {
        ((resident_fn1)(uintptr_t)VF3_RESIDENT_ADDR_DIAG_BEGIN)(0u);
        ((resident_fn_void)(uintptr_t)VF3_RESIDENT_ADDR_CHAINLOAD)();
    }
    for (;;) { }
}
