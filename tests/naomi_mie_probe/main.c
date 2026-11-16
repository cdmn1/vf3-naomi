/*
 * VF3 Naomi MIE/JVS input probe.
 *
 * Build with a KallistiOS toolchain configured for Naomi. This is a
 * standalone hardware probe; it does not boot or modify VF3.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <kos.h>
#include <kos/init.h>
#include <dc/maple.h>
#include <dc/maple/mie.h>

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MIE);

static const char *port_mode_name(mie_port0_mode_t mode)
{
    switch(mode) {
    case MIE_PORT0_JVS:   return "MIE/JVS";
    case MIE_PORT0_MAPLE: return "standard Maple";
    default:              return "unknown / scan pending";
    }
}

static void report_inputs(void)
{
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_MIE);
    mie_state_t *state = dev ? (mie_state_t *)maple_dev_status(dev) : NULL;

    printf("VF3 Naomi MIE/JVS probe\n");
    printf("Port A: %s\n", port_mode_name(mie_port0_mode()));

    if(!state) {
        printf("MIE device: not enumerated\n");
        printf("\n");
        return;
    }

    printf("MIE device: detected\n");
    printf("Mapped P1 buttons: %08" PRIx32 "\n", state->cont.buttons);
    printf("Native JVS: P1=%04x P2=%04x SYS=%02x PANEL=%02x\n",
           state->jvs.p1.raw, state->jvs.p2.raw,
           state->jvs.system.raw, state->jvs.panel.psw.raw);
    printf("Coins: slot0=%u slot1=%u pulses=%02x faults=%02x\n",
           state->jvs.coin[0].count, state->jvs.coin[1].count,
           state->jvs.coin_pulse, state->jvs.coin_fault);
    printf("Analog:");
    for(unsigned i = 0; i < MIE_JVS_ANALOG_CHANNELS; ++i) {
        printf(" %u", state->jvs.analog[i]);
    }
    printf("\n\n");
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("\nVF3 Naomi MIE/JVS probe starting\n");
    maple_wait_scan();
    printf("Initial Maple/MIE scan complete\n");

    for(;;) {
        report_inputs();
        thd_sleep(250);
    }

    return 0;
}
