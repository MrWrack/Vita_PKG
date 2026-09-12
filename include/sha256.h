
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} MrwSha256;

void mrw_sha256_init(MrwSha256 *ctx);
void mrw_sha256_update(MrwSha256 *ctx, const uint8_t *data, size_t len);
void mrw_sha256_final(MrwSha256 *ctx, uint8_t hash[32]);
