#include <kos.h>
#include <stdint.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint32_t vf3_gd_install(void);
extern void vf3_gd_diag_exception_entry(uint32_t stack_top);

static uint8_t capture_stack[4096] __attribute__((aligned(16)));

int main(void)
{
    pvr_init_defaults();

    if (vf3_gd_install() == 0xffffffffu)
        for (;;) thd_sleep(1000);

    /*
     * Exercise the same privileged register-capture sequence used by the
     * exception entry, but use a stack already proven by the v174 test.
     * This deliberately bypasses VBR dispatch and hardware exception entry.
     */
    vf3_gd_diag_exception_entry(
        (uint32_t)(uintptr_t)(capture_stack + sizeof(capture_stack)));

    for (;;) thd_sleep(1000);
    return 0;
}
