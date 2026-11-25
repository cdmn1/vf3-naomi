/* Compare v77's final guest MT outputs against an extracted MT*.BIN pack. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mt.h"
#include "motion.h"

typedef union { float f; uint32_t u; } FloatBits;

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

static int parse_u32(const char *s, int base, uint32_t *out)
{
    char *end = NULL;
    unsigned long value;
    if (!s || !*s) return 0;
    value = strtoul(s, &end, base);
    if (!end || *end != '\0' || value > 0xFFFFFFFFul) return 0;
    *out = (uint32_t)value;
    return 1;
}

int main(int argc, char **argv)
{
    uint8_t *asset = NULL;
    uint32_t asset_size = 0;
    MtFile file;
    FILE *csv;
    char line[8192];
    uint32_t actor_filter = 0, motion_filter = 0, cycle_filter = 0;
    uint64_t rows = 0, load_failures = 0, eval_failures = 0;
    uint64_t exact_calls = 0, exact_channels = 0, total_channels = 0;
    double total_sq = 0.0, max_abs = 0.0;

    if (argc < 3 || argc > 6) {
        fprintf(stderr, "usage: %s MT_FILE.BIN v77-mt-targets.csv [actor] [motion_id] [cycle]\n", argv[0]);
        return 2;
    }
    if (argc >= 4 && !parse_u32(argv[3], 0, &actor_filter)) return 2;
    if (argc >= 5 && !parse_u32(argv[4], 0, &motion_filter)) return 2;
    if (argc >= 6 && !parse_u32(argv[5], 10, &cycle_filter)) return 2;

    if (!read_file(argv[1], &asset, &asset_size)) {
        fprintf(stderr, "could not read MT asset: %s\n", argv[1]);
        return 2;
    }
    if (mt_parse(asset, asset_size, &file) != 0) {
        fprintf(stderr, "MT table parse failed: %s\n", argv[1]);
        free(asset);
        return 1;
    }
    csv = fopen(argv[2], "r");
    if (!csv) {
        fprintf(stderr, "could not read target CSV: %s\n", argv[2]);
        free(asset);
        return 2;
    }

    printf("VF3 v77 MT OUTPUT COMPARISON\n");
    printf("ASSET=%s BYTES=%u POPULATED_SLOTS=%u\n",
           argv[1], asset_size, file.count);
    printf("FILTER_ACTOR=%s FILTER_MOTION=%s FILTER_CYCLE=%s\n",
           actor_filter ? argv[3] : "all",
           motion_filter ? argv[4] : "all",
           cycle_filter ? argv[5] : "all");

    (void)fgets(line, sizeof line, csv); /* header */
    while (fgets(line, sizeof line, csv)) {
        char *fields[80] = {0};
        char *save = NULL;
        char *token;
        unsigned n = 0;
        uint32_t actor, motion_id, cycle, phase;
        MtMotionView view;
        vf3_native_motion_stream stream;
        float actual[VF3_NATIVE_MOTION_CHANNELS];
        unsigned exact = 0;
        double sum_sq = 0.0, call_max = 0.0;
        int rc;

        token = strtok_r(line, ",\r\n", &save);
        while (token && n < 80) {
            fields[n++] = token;
            token = strtok_r(NULL, ",\r\n", &save);
        }
        if (n < 71) continue;
        if (!parse_u32(fields[1], 0, &actor) ||
            !parse_u32(fields[2], 0, &motion_id) ||
            !parse_u32(fields[3], 10, &cycle) ||
            !parse_u32(fields[4], 10, &phase))
            continue;
        if (actor_filter && actor != actor_filter) continue;
        if (motion_filter && motion_id != motion_filter) continue;
        if (cycle_filter && cycle != cycle_filter) continue;
        ++rows;

        rc = vf3_native_motion_load(&file, motion_id, (uint16_t)cycle,
                                    &view, &stream);
        if (rc != 0) {
            ++load_failures;
            if (load_failures <= 5)
                printf("LOAD_FAIL event=%s motion=0x%X cycle=%u rc=%d\n",
                       fields[0], motion_id, cycle, rc);
            continue;
        }
        rc = vf3_native_motion_eval(&stream, (int32_t)phase, actual);
        if (rc != 0) {
            ++eval_failures;
            if (eval_failures <= 5)
                printf("EVAL_FAIL event=%s motion=0x%X cycle=%u phase=%u rc=%d\n",
                       fields[0], motion_id, cycle, phase, rc);
            continue;
        }

        for (unsigned ch = 0; ch < VF3_NATIVE_MOTION_CHANNELS; ++ch) {
            uint32_t expected_bits;
            FloatBits got;
            double delta;
            if (!parse_u32(fields[8 + ch], 16, &expected_bits)) {
                fprintf(stderr, "bad target word at event %s channel %u\n",
                        fields[0], ch);
                fclose(csv);
                free(asset);
                return 1;
            }
            got.f = actual[ch];
            delta = (double)got.f - (double)((FloatBits){.u = expected_bits}).f;
            if (got.u == expected_bits) {
                ++exact;
                ++exact_channels;
            }
            if (fabs(delta) > call_max) call_max = fabs(delta);
            sum_sq += delta * delta;
        }
        total_channels += VF3_NATIVE_MOTION_CHANNELS;
        total_sq += sum_sq;
        if (call_max > max_abs) max_abs = call_max;
        if (exact == VF3_NATIVE_MOTION_CHANNELS) ++exact_calls;
        if (rows <= 5 || exact == VF3_NATIVE_MOTION_CHANNELS) {
            printf("CALL event=%s actor=0x%08X motion=0x%X cycle=%u phase=0x%X exact=%u/63 rmse=%.9g max_abs=%.9g\n",
                   fields[0], actor, motion_id, cycle, phase, exact,
                   sqrt(sum_sq / 63.0), call_max);
        }
    }
    fclose(csv);

    printf("SUMMARY rows_selected=%llu load_failures=%llu eval_failures=%llu\n",
           (unsigned long long)rows,
           (unsigned long long)load_failures,
           (unsigned long long)eval_failures);
    printf("EXACT_CALLS=%llu/%llu EXACT_CHANNELS=%llu/%llu\n",
           (unsigned long long)exact_calls,
           (unsigned long long)(rows - load_failures - eval_failures),
           (unsigned long long)exact_channels,
           (unsigned long long)total_channels);
    if (total_channels)
        printf("OVERALL_RMSE=%.9g MAX_ABS=%.9g\n",
               sqrt(total_sq / (double)total_channels), max_abs);
    free(asset);
    return rows && !load_failures && !eval_failures ? 0 : 1;
}
