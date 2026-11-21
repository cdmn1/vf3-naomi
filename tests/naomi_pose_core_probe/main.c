#include <kos.h>
#include <dc/minifont.h>
#include <dc/video.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "motion_aim.h"
#include "aim_math.h"
#include "matrix.h"
#include "pose_orchestration.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define RAM_BASE 0x0C010000u
#define RAM_SIZE 0x200000u

static float ram[RAM_SIZE / sizeof(float)];
static char lines[8][64];
static int pass_count, fail_count;

static void check(int ok, const char *name)
{
    snprintf(lines[pass_count + fail_count], sizeof lines[0], "%s: %s",
             name, ok ? "PASS" : "FAIL");
    if (ok) ++pass_count; else ++fail_count;
}

static int nearf(float a, float b)
{
    return isfinite(a) && fabsf(a - b) < 0.0005f;
}

int main(void)
{
    float identity[16], rotated[16], result[16];
    const float point[3] = { 2.0f, 3.0f, 4.0f };
    float transformed[3];
    vf3_native_aim aim;
    memset(&aim, 0, sizeof aim);
    aim.ram = ram;
    aim.ram_base = RAM_BASE;
    aim.f = 0x0C200000u;
    vf3_native_matrix_identity(identity);
    check(nearf(identity[0],1) && nearf(identity[5],1) &&
          nearf(identity[10],1) && nearf(identity[15],1) &&
          nearf(identity[1],0) && nearf(identity[14],0),
          "MATRIX IDENTITY");

    vf3_native_matrix_identity(rotated);
    rotated[0] = 0.0f; rotated[1] = 1.0f;
    rotated[4] = -1.0f; rotated[5] = 0.0f;
    vf3_native_matrix_multiply(identity, rotated, result);
    check(nearf(result[0],0) && nearf(result[1],1) &&
          nearf(result[4],-1) && nearf(result[5],0),
          "MATRIX MULTIPLY");

    vf3_native_matrix_transform_point(rotated, point, transformed);
    check(nearf(transformed[0],-3) && nearf(transformed[1],2) &&
          nearf(transformed[2],4),
          "POINT TRANSFORM");

    memcpy(aim.xm, rotated, sizeof rotated);
    vf3_aim_push(&aim, 0);
    vf3_aim_identity(&aim);
    vf3_aim_pop(&aim);
    check(aim.sp == 0 && nearf(aim.xm[0],0) && nearf(aim.xm[1],1) &&
          nearf(aim.xm[4],-1) && nearf(aim.xm[5],0),
          "POSE MATRIX STACK");

    vf3_native_matrix_multiply(rotated, rotated, result);
    check(nearf(result[0],-1) && nearf(result[5],-1) &&
          nearf(result[10],1) && nearf(result[15],1),
          "COMPOSED ROTATION");

    check(sizeof(vf3_native_aim) >= sizeof(float) * (16*16 + 16 + 16),
          "POSE CONTEXT LAYOUT");

    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        vid_clear(0, 0, 0);
        bfont_draw_str(vram_s + 640 * 12 + 16, 640, 1,
                       "VF3 NAOMI - NATIVE POSE CORE");
        for (int i = 0; i < pass_count + fail_count; ++i)
            bfont_draw_str(vram_s + 640 * (48 + i * 24) + 16, 640, 1, lines[i]);
        char summary[64];
        snprintf(summary, sizeof summary, "RESULT: %d PASS / %d FAIL",
                 pass_count, fail_count);
        bfont_draw_str(vram_s + 640 * 420 + 16, 640, 1, summary);
        vid_flip(-1);
        thd_sleep(100);
    }
    return 0;
}
