#include "vf3_input_map.h"

#include <stdio.h>

static int check(const char *name, uint16_t jvs, uint16_t expected)
{
    const uint16_t actual = vf3_jvs_to_maple_buttons(jvs);
    if(actual != expected) {
        fprintf(stderr, "%s: expected %04x, got %04x\n", name, expected, actual);
        return 1;
    }
    if((actual & 0xF901u) != 0xF901u) {
        fprintf(stderr, "%s: unsupported pad bits were not released\n", name);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failed = 0;
    failed |= check("neutral", 0x0000u, 0xFFFFu);
    failed |= check("punch",  1u << 9,  0xFFFBu);
    failed |= check("kick",   1u << 8,  0xFFFDu);
    failed |= check("guard",  1u << 0,  0xFBFFu);
    failed |= check("escape", 1u << 1,  0xFDFFu);
    failed |= check("start",  1u << 15, 0xFFF7u);
    failed |= check("up",     1u << 13, 0xFFEFu);
    failed |= check("down",   1u << 12, 0xFFDFu);
    failed |= check("left",   1u << 11, 0xFFBFu);
    failed |= check("right",  1u << 10, 0xFF7Fu);
    failed |= check("all VF3 controls", 0xFF03u, 0xF901u);
    if(failed) return 1;
    puts("vf3_input_map: JVS inputs encode as active-low Dreamcast Maple buttons");
    return 0;
}
