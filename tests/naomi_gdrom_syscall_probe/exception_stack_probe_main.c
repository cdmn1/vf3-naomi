#include <kos.h>
#include <stdint.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint32_t vf3_gd_install(void);
extern void vf3_gd_diag_exception_stack(uint32_t stack_top);

static uint8_t safe_exception_stack[4096] __attribute__((aligned(16)));

int main(void)
{
    pvr_init_defaults();

    if (vf3_gd_install() == 0xffffffffu)
        for (;;) thd_sleep(1000);

    /* Switch to a 16-byte-aligned stack allocated in the KOS app's BSS, then
       call the known-good renderer. This isolates stack address validity from
       the resident stack-switch instruction. Register capture is omitted. */
    vf3_gd_diag_exception_stack((uint32_t)(uintptr_t)(safe_exception_stack + sizeof(safe_exception_stack)));

    for (;;) thd_sleep(1000);
    return 0;
}
