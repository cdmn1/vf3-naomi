/* Default probe map. CI replaces this with a generated retail asset map. */
#ifndef VF3_ASSET_MAP_H
#define VF3_ASSET_MAP_H
#include <stdint.h>
typedef struct { uint32_t fad_start, sectors, payload_offset; } vf3_asset_range;
#define VF3_ASSET_MAP_COUNT 0u
#define VF3_ASSET_PAYLOAD_BASE 0u
static const vf3_asset_range vf3_asset_map[1] = {{0u,0u,0u}};
#endif
