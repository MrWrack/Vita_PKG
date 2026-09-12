
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} MrwSha1;
void mrw_sha1_init(MrwSha1 *ctx);
void mrw_sha1_update(MrwSha1 *ctx, const void *data, size_t len);
void mrw_sha1_final(MrwSha1 *ctx, uint8_t out[20]);
