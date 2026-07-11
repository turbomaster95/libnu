#include <nu.h>
#include <intnu.h>

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} nu_sha256_ctx_t;

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_transform(nu_sha256_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7], w[64];
    
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8)  | ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR32(w[i-15], 7) ^ ROR32(w[i-15], 18) ^ SHR(w[i-15], 3);
        uint32_t s1 = ROR32(w[i-2], 17) ^ ROR32(w[i-2], 19) ^ SHR(w[i-2], 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25), ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + sha256_k[i] + w[i];
        uint32_t S0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22), maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

char* nu_internal_sha256(nu_mm_t *mm, const uint8_t *data, size_t len) {
    nu_sha256_ctx_t ctx = { .count = 0, .state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} };
    size_t idx = 0;
    
    for (size_t i = 0; i < len; i++) {
        ctx.buffer[idx++] = data[i];
        if (idx == 64) { sha256_transform(&ctx, ctx.buffer); ctx.count += 512; idx = 0; }
    }
    ctx.count += (idx << 3);
    
    ctx.buffer[idx++] = 0x80;
    if (idx > 56) {
        while (idx < 64) ctx.buffer[idx++] = 0x00;
        sha256_transform(&ctx, ctx.buffer); idx = 0;
    }
    while (idx < 56) ctx.buffer[idx++] = 0x00;
    for (int i = 0; i < 8; i++) ctx.buffer[56 + i] = (uint8_t)(ctx.count >> (56 - i * 8));
    sha256_transform(&ctx, ctx.buffer);
    
    uint8_t digest[32];
    for (int i = 0; i < 8; i++) {
        digest[i*4]   = (uint8_t)(ctx.state[i] >> 24); digest[i*4+1] = (uint8_t)(ctx.state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx.state[i] >> 8);  digest[i*4+3] = (uint8_t)(ctx.state[i]);
    }
    return nu_digest_to_hex(mm, digest, 32);
}

