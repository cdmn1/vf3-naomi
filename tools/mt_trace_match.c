/* Match retail 42-float pose-channel samples to the MTAKIPAI MT asset. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mt.h"
#include "motion.h"

#define TARGET_COUNT 42u
#define CHANNEL_START 21u
#define MOTION_ID_LIMIT 2960u
#define COARSE_STEP 0x10u
#define TOP_COUNT 12u
static const uint32_t cycles[] = {24u, 30u, 36u, 40u, 45u, 48u,
                                  60u, 72u, 90u, 120u, 180u, 240u};

typedef union { float f; uint32_t u; } FloatBits;

typedef struct {
    uint32_t motion_id;
    uint32_t cycle;
    uint32_t phase;
    unsigned exact;
    double rmse;
    double max_abs;
} Candidate;

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

static Candidate score_phase(const vf3_native_motion_stream *stream,
                             uint32_t motion_id, uint32_t cycle,
                             uint32_t phase,
                             const uint32_t target[TARGET_COUNT])
{
    float values[VF3_NATIVE_MOTION_CHANNELS];
    Candidate c;
    double sum_sq = 0.0;
    c.motion_id = motion_id;
    c.cycle = cycle;
    c.phase = phase;
    c.exact = 0;
    c.max_abs = 0.0;
    c.rmse = HUGE_VAL;
    if (vf3_native_motion_eval(stream, (int32_t)phase, values) != 0)
        return c;
    for (unsigned i = 0; i < TARGET_COUNT; ++i) {
        FloatBits actual, expected;
        double delta;
        actual.f = values[CHANNEL_START + i];
        expected.u = target[i];
        delta = (double)actual.f - (double)expected.f;
        if (actual.u == target[i]) ++c.exact;
        if (fabs(delta) > c.max_abs) c.max_abs = fabs(delta);
        sum_sq += delta * delta;
    }
    c.rmse = sqrt(sum_sq / (double)TARGET_COUNT);
    return c;
}

static int better(const Candidate *a, const Candidate *b)
{
    if (a->rmse < b->rmse) return 1;
    if (a->rmse > b->rmse) return 0;
    return a->exact > b->exact;
}

static void insert_top(Candidate top[TOP_COUNT], unsigned *count,
                       const Candidate *candidate)
{
    unsigned pos = 0;
    while (pos < *count && !better(candidate, &top[pos])) ++pos;
    if (pos >= TOP_COUNT) return;
    if (*count < TOP_COUNT) ++*count;
    for (unsigned i = *count - 1; i > pos; --i) top[i] = top[i - 1];
    top[pos] = *candidate;
}

static Candidate scan_motion_cycle(const MtFile *file, uint32_t motion_id,
                                   uint32_t cycle,
                                   const uint32_t target[TARGET_COUNT])
{
    MtMotionView view;
    vf3_native_motion_stream stream;
    Candidate best = {motion_id, cycle, 0, 0, HUGE_VAL, HUGE_VAL};
    uint32_t limit = cycle * 256u;
    if (vf3_native_motion_load(file, motion_id, cycle, &view, &stream) != 0)
        return best;

    for (uint32_t phase = 0; phase < limit; phase += COARSE_STEP) {
        Candidate current = score_phase(&stream, motion_id, cycle,
                                        phase, target);
        if (better(&current, &best)) best = current;
    }

    if (best.rmse < HUGE_VAL) {
        uint32_t start = best.phase > COARSE_STEP
                       ? best.phase - COARSE_STEP : 0;
        uint32_t end = best.phase + COARSE_STEP;
        if (end >= limit) end = limit - 1;
        for (uint32_t phase = start; phase <= end; ++phase) {
            Candidate current = score_phase(&stream, motion_id, cycle,
                                            phase, target);
            if (better(&current, &best)) best = current;
        }
    }
    return best;
}

static void match_all(const MtFile *file,
                      const uint32_t target[TARGET_COUNT])
{
    Candidate coarse_top[TOP_COUNT];
    unsigned coarse_count = 0;
    unsigned valid_motions = 0;
    MtMotionView view;
    vf3_native_motion_stream stream;

    /* Find likely motion IDs using the established 60-frame cycle first. */
    for (uint32_t motion_id = 1; motion_id <= MOTION_ID_LIMIT; ++motion_id) {
        Candidate best = {motion_id, 60u, 0, 0, HUGE_VAL, HUGE_VAL};
        if (vf3_native_motion_load(file, motion_id, 60u,
                                   &view, &stream) != 0)
            continue;
        ++valid_motions;
        for (uint32_t phase = 0; phase < 60u * 256u; phase += COARSE_STEP) {
            Candidate current = score_phase(&stream, motion_id, 60u,
                                            phase, target);
            if (better(&current, &best)) best = current;
        }
        insert_top(coarse_top, &coarse_count, &best);
    }

    printf("VALID_MOTIONS=%u COARSE_CYCLE=60 COARSE_PHASE_STEP=0x%X\n",
           valid_motions, COARSE_STEP);

    /* Re-test the strongest IDs with multiple possible retail cycle values. */
    Candidate final_top[TOP_COUNT];
    unsigned final_count = 0;
    for (unsigned i = 0; i < coarse_count; ++i) {
        Candidate best_for_motion = coarse_top[i];
        for (unsigned ci = 0; ci < sizeof(cycles) / sizeof(cycles[0]); ++ci) {
            Candidate current = scan_motion_cycle(file,
                coarse_top[i].motion_id, cycles[ci], target);
            if (better(&current, &best_for_motion))
                best_for_motion = current;
        }
        insert_top(final_top, &final_count, &best_for_motion);
    }

    for (unsigned i = 0; i < final_count; ++i) {
        printf("RANK=%u MOTION=%u CYCLE=%u PHASE=0x%04X EXACT_BITS=%u/42 "
               "RMSE=%.9g MAX_ABS=%.9g\n",
               i + 1, final_top[i].motion_id, final_top[i].cycle,
               (unsigned)final_top[i].phase, final_top[i].exact,
               final_top[i].rmse, final_top[i].max_abs);
    }
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
    match_all(&file, target);
    free(data);
    return 0;
}
