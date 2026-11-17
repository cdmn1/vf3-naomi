#ifndef NAOMI_BIOS_H
#define NAOMI_BIOS_H

#include <stdbool.h>

/*
 * Naomi retail BIOS service access.
 *
 * Call naomi_bios_helpers_init() once during application startup, before
 * attempting to hand control back to the Naomi System Test Menu.
 */
bool naomi_bios_helpers_init(void);
bool naomi_bios_helpers_ready(void);

/*
 * Returns true once for each rising edge of the physical JVS system TEST
 * switch. The caller remains responsible for stopping game-specific work
 * (network, sound, DMA queues, Naomi 2 ELAN activity) before handing off.
 */
bool naomi_bios_test_switch_update(bool pressed);

/*
 * Transfers control to the Naomi BIOS System Test Menu through BIOS helper
 * entry 0x44. This routine does not return.
 */
#if defined(__GNUC__)
__attribute__((noreturn))
#endif
void naomi_bios_enter_system_test(void);

#endif
