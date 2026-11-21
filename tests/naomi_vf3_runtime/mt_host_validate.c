/* Host-side verification of the exact MT parser/evaluator used by the Naomi ROM. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "mt.h"
#include "motion.h"

static int check_motion(const MtFile *file, uint32_t motion_id)
{
    static const int32_t phases[] = {0x0100, 0x1000, 0x2000, 0x3B00};
    MtMotionView storage;
    vf3_native_motion_stream stream;
    float values[VF3_NATIVE_MOTION_CHANNELS];
    unsigned op_count[4] = {0, 0, 0, 0};
    int rc = vf3_native_motion_load(file, motion_id, 60, &storage, &stream);
    if (rc != 0) {
        fprintf(stderr, "motion %u: source load failed (%d)\n", motion_id, rc);
        return 0;
    }
    for (unsigned i = 0; i < VF3_NATIVE_MOTION_CHANNELS; ++i) {
        if (storage.ops[i] > 3u) return 0;
        ++op_count[storage.ops[i]];
    }
    printf("MOTION %u WORD0=%u OFFSETS=%06X,%06X,%06X\n",
           motion_id, (unsigned)storage.descriptor_word,
           storage.source_offsets[0], storage.source_offsets[1],
           storage.source_offsets[2]);
    printf("  OPS=%u,%u,%u,%u STREAMS=%lu,%lu,%lu\n",
           op_count[0], op_count[1], op_count[2], op_count[3],
           (unsigned long)storage.counts_size,
           (unsigned long)storage.times_size,
           (unsigned long)storage.tuples_size);
    for (unsigned p = 0; p < sizeof(phases) / sizeof(phases[0]); ++p) {
        float minimum = 0.0f, maximum = 0.0f;
        rc = vf3_native_motion_eval(&stream, phases[p], values);
        if (rc != 0) {
            fprintf(stderr, "motion %u phase %04X: eval failed (%d)\n",
                    motion_id, (unsigned)phases[p], rc);
            return 0;
        }
        for (unsigned ch = 0; ch < VF3_NATIVE_MOTION_CHANNELS; ++ch) {
            const float value = values[ch];
            if (!isfinite(value)) {
                fprintf(stderr, "motion %u phase %04X: channel %u non-finite\n",
                        motion_id, (unsigned)phases[p], ch);
                return 0;
            }
            if (ch == 0 || value < minimum) minimum = value;
            if (ch == 0 || value > maximum) maximum = value;
        }
        printf("  PHASE=%04X RANGE=%.4f..%.4f PASS\n",
               (unsigned)phases[p], minimum, maximum);
    }
    return 1;
}

int main(int argc, char **argv)
{
    FILE *fp;
    long length;
    uint8_t *data;
    MtFile file;
    int ok;
    if (argc != 2) {
        fprintf(stderr, "usage: %s MTAKIPAI.BIN\n", argv[0]);
        return 2;
    }
    fp = fopen(argv[1], "rb");
    if (!fp || fseek(fp, 0, SEEK_END) != 0 ||
        (length = ftell(fp)) <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "could not read asset\n");
        if (fp) fclose(fp);
        return 2;
    }
    data = (uint8_t *)malloc((size_t)length);
    if (!data || fread(data, 1, (size_t)length, fp) != (size_t)length) {
        fprintf(stderr, "asset read/allocation failed\n");
        free(data);
        fclose(fp);
        return 2;
    }
    fclose(fp);
    if (mt_parse(data, (uint32_t)length, &file) != 0) {
        fprintf(stderr, "MT table parse failed\n");
        free(data);
        return 1;
    }
    printf("ASSET_BYTES=%ld POPULATED_SLOTS=%lu FIRST_RECORD=%06X\n",
           length, (unsigned long)file.count, file.first_record_off);
    ok = file.count == 2280u && file.first_record_off == MT_PAYLOAD_FLOOR &&
         check_motion(&file, 1050u) && check_motion(&file, 2386u);
    printf("REAL_MTAKIPAI_SOURCE_EVALUATION: %s\n", ok ? "PASS" : "FAIL");
    free(data);
    return ok ? 0 : 1;
}
