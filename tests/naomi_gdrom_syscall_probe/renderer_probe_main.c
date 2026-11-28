#include <kos.h>
#include <dc/pvr.h>
#include <stdint.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint32_t vf3_gd_install(void);
extern void vf3_gd_diag_exception_display(void);

int main(void)
{
    pvr_init_defaults();

    if (vf3_gd_install() == 0xffffffffu)
        for (;;) thd_sleep(1000);

    /* Call the exact resident exception display function directly.
       This deliberately bypasses VBR/vector dispatch and the exception stack. */
    vf3_gd_diag_exception_display();

    for (;;) thd_sleep(1000);
    return 0;
}
