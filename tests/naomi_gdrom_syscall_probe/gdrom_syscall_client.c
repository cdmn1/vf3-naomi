#include <stddef.h>
#include <stdint.h>
#include "gdrom_syscall.h"
#include "resident_offsets.h"

extern const uint8_t resident_blob_start[] __asm__("_binary_resident_shim_bin_start");
extern const uint8_t resident_blob_end[] __asm__("_binary_resident_shim_bin_end");

typedef uint32_t (*resident_fn0)(void);

static int resident_loaded;

static int load_resident_image(void)
{
    const uint8_t *src = resident_blob_start;
    size_t size = (size_t)(resident_blob_end - resident_blob_start);
    volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)0xad000000u;
    uintptr_t line;

    if (resident_loaded)
        return 0;
    if (size != VF3_RESIDENT_IMAGE_SIZE)
        return -1;

    /* P2 is the uncached alias of the reserved 0x8Dxxxxxx RAM window. */
    for (size_t i = 0; i < size; ++i)
        dst[i] = src[i];

    /* Invalidate stale data/instructions for the copied resident image. */
    for (line = VF3_RESIDENT_IMAGE_BASE;
         line < VF3_RESIDENT_IMAGE_BASE + size; line += 32u) {
        __asm__ volatile("ocbi @%0" :: "r"(line) : "memory");
        __asm__ volatile("icbi @%0" :: "r"(line) : "memory");
    }
    __asm__ volatile("synco" ::: "memory");
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
