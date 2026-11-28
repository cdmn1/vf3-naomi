#include <kos.h>
#include <stdint.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint32_t vf3_gd_install(void);
extern void vf3_gd_diag_exception_entry(void);

int main(void)
{
    pvr_init_defaults();

    if (vf3_gd_install() == 0xffffffffu)
        for (;;) thd_sleep(1000);

    /* Invoke the actual resident exception-entry assembly directly.
       This bypasses VBR dispatch but exercises its stack switch,
       register capture and call into the exception display routine. */
    vf3_gd_diag_exception_entry();

    for (;;) thd_sleep(1000);
    return 0;
}
