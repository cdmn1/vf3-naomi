/* Match a retail 42-float pose-channel sample to the real MTAKIPAI MT asset. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mt.h"
#include "motion.h"

#define TARGET_COUNT 42u
#define CHANNEL_START 21u
#define TEST_CYCLE 60u
#define PHASE_LIMIT (TEST_CYCLE * 256u)

typedef union {
    float f;
    uint32_t u;
} FloatBits;

static int read_file(const char *path, uint8_t **data, uint32_t *size)
{
    FILE *fp = fopen(path, "rb");
    long length;
    uint8_t *buffer;
    if (!fp || fseek(fp, 0, SEEK_END) != 0 ||
        (length = ftell(fp)) <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
        if (fp) fclose(fp);
        return 0;
    }
    buffer = (uint8_t *)malloc((size_t)length);
    if (!buffer || fread(buffer, 1, (size_t)length, fp) != (size_t)length) {
        free(buffer);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    *data = buffer;
    *size = (uint32_t)length;
    return 1;
}

static int read_target(const char *path, uint32_t target[TARGET_COUNT])
{
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    for (unsigned i = 0; i < TARGET_COUNT; ++i) {
        unsigned value;
        if (fscanf(fp, "%x", &value) != 1) {
            fclose(fp);
            return 0;
        }
        target[i] = (uint32_t)value;
    }
    fclose(fp);
    return 1;
}

static void match_motion(const MtFile *file, uint32_t motion_id,
                         const uint32_t target[TARGET_COUNT])
{
    MtMotionView view;
    vf3_native_motion_stream stream;
    float values[VF3_NATIVE_MOTION_CHANNELS];
    double best_rmse = HUGE_VAL;
    double best_max = HUGE_VAL;
    uint32_t best_phase = 0;
    unsigned best_exact = 0;
    unsigned exact_phases = 0;
    int rc = vf3_native_motion_load(file, motion_id, TEST_CYCLE,
                                   &view, &stream);
    if (rc != 0) {
        printf("MOTION %u: LOAD ERROR %d\n", motion_id, rc);
        return;
    }

    for (uint32_t phase = 0; phase < PHASE_LIMIT; ++phase) {
        double sum_sq = 0.0;
        double max_abs = 0.0;
        unsigned exact = 0;
        rc = vf3_native_motion_eval(&stream, (int32_t)phase, values);
        if (rc != 0) continue;
        for (unsigned i = 0; i < TARGET_COUNT; ++i) {
            FloatBits actual;
            double delta;
            actual.f = values[CHANNEL_START + i];
            delta = (double)actual.f -
                    (double)(FloatBits){ .u = target[i] }.f;
            if (actual.u == target[i]) ++exact;
            if (fabs(delta) > max_abs) max_abs = fabs(delta);
            sum_sq += delta * delta;
        }
        if (exact == TARGET_COUNT) ++exact_phases;
        {
            double rmse = sqrt(sum_sq / (double)TARGET_COUNT);
            if (rmse < best_rmse ||
                (rmse == best_rmse && exact > best_exact)) {
                best_rmse = rmse;
                best_max = max_abs;
                best_phase = phase;
                best_exact = exact;
            }
        }
    }

    printf("MOTION %u BEST_PHASE=0x%04X EXACT_BITS=%u/42 "
           "RMSE=%.9g MAX_ABS=%.9g EXACT_PHASES=%u\n",
           motion_id, (unsigned)best_phase, best_exact,
           best_rmse, best_max, exact_phases);
}

int main(int argc, char **argv)
{
    uint8_t *data = NULL;
    uint32_t size = 0;
    uint32_t target[TARGET_COUNT];
    MtFile file;
    if (argc != 3) {
        fprintf(stderr, "usage: %s MTAKIPAI.BIN target42.txt\n", argv[0]);
        return 2;
    }
    if (!read_file(argv[1], &data, &size) ||
        !read_target(argv[2], target)) {
        fprintf(stderr, "could not read asset or 42-channel target\n");
        free(data);
        return 2;
    }
    if (mt_parse(data, size, &file) != 0) {
        fprintf(stderr, "MTAKIPAI table parse failed\n");
        free(data);
        return 1;
    }
    printf("ASSET_BYTES=%u POPULATED_SLOTS=%lu TARGET=%s\n",
           size, (unsigned long)file.count, argv[2]);
    match_motion(&file, 1050u, target);
    match_motion(&file, 2386u, target);
    free(data);
    return 0;
}
