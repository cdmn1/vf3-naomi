#include <kos.h>
#include <stdint.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint32_t vf3_gd_install(void);
extern void vf3_gd_diag_exception_stack(void);

int main(void)
{
    pvr_init_defaults();

    if (vf3_gd_install() == 0xffffffffu)
        for (;;) thd_sleep(1000);

    /* Switch to the exact exception stack, then call the known-good renderer.
       This deliberately omits SPC/SSR/SGR/EXPEVT/TEA/SR capture. */
    vf3_gd_diag_exception_stack();

    for (;;) thd_sleep(1000);
    return 0;
}
