#include "vf3_input_map.h"

#define JVS_PUSH1   (1u << 9)
#define JVS_PUSH2   (1u << 8)
#define JVS_PUSH3   (1u << 0)
#define JVS_PUSH4   (1u << 1)
#define JVS_RIGHT   (1u << 10)
#define JVS_LEFT    (1u << 11)
#define JVS_DOWN    (1u << 12)
#define JVS_UP      (1u << 13)
#define JVS_START   (1u << 15)

#define DC_BTN_B     0x0002u
#define DC_BTN_A     0x0004u
#define DC_BTN_START 0x0008u
#define DC_BTN_UP    0x0010u
#define DC_BTN_DOWN  0x0020u
#define DC_BTN_LEFT  0x0040u
#define DC_BTN_RIGHT 0x0080u
#define DC_BTN_Y     0x0200u
#define DC_BTN_X     0x0400u
#define DC_STANDARD_PAD_RELEASED_MASK 0xF901u

uint16_t vf3_jvs_to_maple_buttons(uint16_t jvs_switches)
{
    uint16_t pressed = 0;

    if(jvs_switches & JVS_PUSH1) pressed |= DC_BTN_A;       /* Punch */
    if(jvs_switches & JVS_PUSH2) pressed |= DC_BTN_B;       /* Kick */
    if(jvs_switches & JVS_PUSH3) pressed |= DC_BTN_X;       /* Guard */
    if(jvs_switches & JVS_PUSH4) pressed |= DC_BTN_Y;       /* Escape */
    if(jvs_switches & JVS_START) pressed |= DC_BTN_START;
    if(jvs_switches & JVS_UP)    pressed |= DC_BTN_UP;
    if(jvs_switches & JVS_DOWN)  pressed |= DC_BTN_DOWN;
    if(jvs_switches & JVS_LEFT)  pressed |= DC_BTN_LEFT;
    if(jvs_switches & JVS_RIGHT) pressed |= DC_BTN_RIGHT;

    /* The guest sees a standard controller: zero means pressed. */
    return (uint16_t)((~pressed) | DC_STANDARD_PAD_RELEASED_MASK);
}
