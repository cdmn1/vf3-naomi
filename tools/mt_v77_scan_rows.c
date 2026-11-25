/* Scan every populated MT row against v77's captured Akira 0x41A outputs.
 * This is a diagnostic: it tests whether the mismatch is explained by a
 * different row selection within MTAKIJAC.BIN, using the exact captured Q8
 * phases and runtime cycle. It does not assume that another row is correct.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mt.h"
#include "motion.h"

#define MAX_TARGETS 700
#define MAX_ROWS MT_MOTION_ROWS

typedef union { float f; uint32_t u; } FloatBits;
typedef struct {
    uint32_t phase;
    uint16_t cycle;
    uint32_t words[VF3_NATIVE_MOTION_CHANNELS];
} Target;
typedef struct {
    uint32_t id;
    uint64_t exact;
    uint64_t total;
    uint64_t exact_calls;
    uint32_t valid_calls;
    double sum_sq;
    double max_abs;
} Score;

static int parse_u32(const char *s, int base, uint32_t *out)
{
    char *end = NULL;
    unsigned long v;
    if (!s || !*s) return 0;
    v = strtoul(s, &end, base);
    if (!end || *end || v > 0xFFFFFFFFul) return 0;
    *out = (uint32_t)v;
    return 1;
}

static int cmp_score(const void *a, const void *b)
{
    const Score *x = (const Score *)a, *y = (const Score *)b;
    double xr, yr;
    if (x->valid_calls != y->valid_calls)
        return x->valid_calls > y->valid_calls ? -1 : 1;
    xr = x->total ? sqrt(x->sum_sq / (double)x->total) : HUGE_VAL;
    yr = y->total ? sqrt(y->sum_sq / (double)y->total) : HUGE_VAL;
    if (xr < yr) return -1;
    if (xr > yr) return 1;
    if (x->exact > y->exact) return -1;
    if (x->exact < y->exact) return 1;
    return x->id < y->id ? -1 : x->id > y->id;
}

int main(int argc, char **argv)
{
    uint8_t *asset = NULL;
    uint32_t asset_size = 0, actor_filter, motion_filter, cycle_filter;
    MtFile file;
    Target targets[MAX_TARGETS];
    Score scores[MAX_ROWS];
    size_t target_count = 0, score_count = 0;
    char line[8192];
    FILE *csv;

    if (argc != 6 ||
        !parse_u32(argv[3], 0, &actor_filter) ||
        !parse_u32(argv[4], 0, &motion_filter) ||
        !parse_u32(argv[5], 10, &cycle_filter)) {
        fprintf(stderr, "usage: %s MT_FILE.BIN targets.csv actor motion_id cycle\n", argv[0]);
        return 2;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp || fseek(fp, 0, SEEK_END) || (long)(asset_size = (uint32_t)ftell(fp)) <= 0 ||
        fseek(fp, 0, SEEK_SET)) {
        if (fp) fclose(fp);
        fprintf(stderr, "cannot open MT asset\n");
        return 2;
    }
    asset = (uint8_t *)malloc(asset_size);
    if (!asset || fread(asset, 1, asset_size, fp) != asset_size) {
        fclose(fp); free(asset);
        fprintf(stderr, "cannot read MT asset\n");
        return 2;
    }
    fclose(fp);
    if (mt_parse(asset, asset_size, &file) != 0) {
        free(asset);
        fprintf(stderr, "MT table parse failed\n");
        return 1;
    }
    csv = fopen(argv[2], "r");
    if (!csv) { free(asset); return 2; }
    (void)fgets(line, sizeof line, csv);
    while (fgets(line, sizeof line, csv)) {
        char *fields[80] = {0}, *save = NULL, *tok;
        unsigned n = 0;
        uint32_t actor, motion, cycle, phase;
        tok = strtok_r(line, ",\r\n", &save);
        while (tok && n < 80) { fields[n++] = tok; tok = strtok_r(NULL, ",\r\n", &save); }
        if (n < 71 || !parse_u32(fields[1], 0, &actor) ||
            !parse_u32(fields[2], 0, &motion) ||
            !parse_u32(fields[3], 10, &cycle) ||
            !parse_u32(fields[4], 10, &phase)) continue;
        if (actor != actor_filter || motion != motion_filter || cycle != cycle_filter) continue;
        if (target_count >= MAX_TARGETS) { fclose(csv); free(asset); return 2; }
        targets[target_count].phase = phase;
        targets[target_count].cycle = (uint16_t)cycle;
        for (unsigned ch = 0; ch < VF3_NATIVE_MOTION_CHANNELS; ++ch) {
            if (!parse_u32(fields[8 + ch], 16, &targets[target_count].words[ch])) {
                fclose(csv); free(asset); return 1;
            }
        }
        ++target_count;
    }
    fclose(csv);
    if (!target_count) { free(asset); fprintf(stderr, "no matching target rows\n"); return 1; }

    printf("VF3 v77 ALL-ROW MT SCAN\nASSET=%s BYTES=%u POPULATED_SLOTS=%u\n",
           argv[1], asset_size, file.count);
    printf("TARGET_ACTOR=0x%08X TARGET_MOTION=0x%X TARGET_CYCLE=%u CALLS=%zu\n",
           actor_filter, motion_filter, cycle_filter, target_count);

    for (uint32_t id = 1; id <= MT_MOTION_ROWS; ++id) {
        MtMotionView view;
        vf3_native_motion_stream stream;
        Score s = {0};
        s.id = id;
        if (vf3_native_motion_load(&file, id, (uint16_t)cycle_filter, &view, &stream) != 0)
            continue;
        for (size_t row = 0; row < target_count; ++row) {
            float actual[VF3_NATIVE_MOTION_CHANNELS];
            if (vf3_native_motion_eval(&stream, (int32_t)targets[row].phase, actual) != 0)
                continue;
            ++s.valid_calls;
            unsigned exact_this = 0;
            for (unsigned ch = 0; ch < VF3_NATIVE_MOTION_CHANNELS; ++ch) {
                FloatBits got;
                got.f = actual[ch];
                const uint32_t want = targets[row].words[ch];
                const float expected = ((FloatBits){.u = want}).f;
                const double delta = (double)got.f - (double)expected;
                const double ad = fabs(delta);
                if (got.u == want) { ++s.exact; ++exact_this; }
                s.sum_sq += delta * delta;
                if (ad > s.max_abs) s.max_abs = ad;
            }
            s.total += VF3_NATIVE_MOTION_CHANNELS;
            if (exact_this == VF3_NATIVE_MOTION_CHANNELS) ++s.exact_calls;
        }
        if (s.valid_calls == target_count)
            scores[score_count++] = s;
    }
    qsort(scores, score_count, sizeof scores[0], cmp_score);
    printf("FULL_COVERAGE_ROWS=%zu\nTOP_20_ROWS_BY_RMSE\n", score_count);
    for (size_t i = 0; i < score_count && i < 20; ++i) {
        const Score *s = &scores[i];
        printf("RANK=%zu ID=0x%03X EXACT_CALLS=%llu/%zu EXACT_CHANNELS=%llu/%llu RMSE=%.9g MAX_ABS=%.9g\n",
               i + 1, s->id, (unsigned long long)s->exact_calls, target_count,
               (unsigned long long)s->exact, (unsigned long long)s->total,
               sqrt(s->sum_sq / (double)s->total), s->max_abs);
    }
    for (size_t i = 0; i < score_count; ++i) {
        if (scores[i].id == motion_filter) {
            printf("REQUESTED_ID_RANK=%zu ID=0x%03X EXACT_CALLS=%llu/%zu EXACT_CHANNELS=%llu/%llu RMSE=%.9g MAX_ABS=%.9g\n",
                   i + 1, scores[i].id, (unsigned long long)scores[i].exact_calls,
                   target_count, (unsigned long long)scores[i].exact,
                   (unsigned long long)scores[i].total,
                   sqrt(scores[i].sum_sq / (double)scores[i].total), scores[i].max_abs);
            break;
        }
    }
    free(asset);
    return score_count ? 0 : 1;
}
