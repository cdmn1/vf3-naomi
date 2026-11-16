#ifndef VF3_NAOMI_INPUT_MAP_H
#define VF3_NAOMI_INPUT_MAP_H

#include <stdint.h>

/* Convert one native JVS player switch word to a standard Dreamcast
 * controller button word. JVS is active-high; Maple is active-low.
 *
 * Default VF3 layout (provisional, cabinet wiring can differ):
 *   JVS push 1 -> Punch (A), push 2 -> Kick (B),
 *   push 3 -> Guard (X), push 4 -> Escape (Y).
 * Directions and Start use the standard JVS switch positions.
 * Unsupported standard-pad buttons remain released (Flycast mask 0xF901).
 */
uint16_t vf3_jvs_to_maple_buttons(uint16_t jvs_switches);

#endif
