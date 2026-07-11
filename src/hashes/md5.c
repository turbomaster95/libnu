#include <nu.h>
#include <intnu.h>

typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} nu_md5_ctx_t;

static const uint32_t md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const uint8_t md5_s[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

static void md5_transform(nu_md5_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3], x[16];
    for (int i = 0; i < 16; i++) {
        x[i] = ((uint32_t)block[i*4]) | ((uint32_t)block[i*4+1] << 8) |
               ((uint32_t)block[i*4+2] << 16) | ((uint32_t)block[i*4+3] << 24);
    }
    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
        else { f = c ^ (b | ~d); g = (7 * i) % 16; }
        uint32_t temp = d; d = c; c = b;
        b = b + ROL32(a + f + md5_k[i] + x[g], md5_s[i]); a = temp;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
}

char* nu_internal_md5(nu_mm_t *mm, const uint8_t *data, size_t len) {
    nu_md5_ctx_t ctx = { .count = 0, .state = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476} };
    size_t idx = 0;
    
    for (size_t i = 0; i < len; i++) {
        ctx.buffer[idx++] = data[i];
        if (idx == 64) { md5_transform(&ctx, ctx.buffer); ctx.count += 512; idx = 0; }
    }
    ctx.count += (idx << 3);
    
    uint8_t bits[8];
    for (int i = 0; i < 8; i++) bits[i] = (uint8_t)(ctx.count >> (i * 8));
    
    ctx.buffer[idx++] = 0x80;
    if (idx > 56) {
        while (idx < 64) ctx.buffer[idx++] = 0x00;
        md5_transform(&ctx, ctx.buffer); idx = 0;
    }
    while (idx < 56) ctx.buffer[idx++] = 0x00;
    for (int i = 0; i < 8; i++) ctx.buffer[56 + i] = bits[i];
    md5_transform(&ctx, ctx.buffer);
    
    uint8_t digest[16];
    for (int i = 0; i < 4; i++) {
        digest[i*4]   = (uint8_t)(ctx.state[i]);
        digest[i*4+1] = (uint8_t)(ctx.state[i] >> 8);
        digest[i*4+2] = (uint8_t)(ctx.state[i] >> 16);
        digest[i*4+3] = (uint8_t)(ctx.state[i] >> 24);
    }
    return nu_digest_to_hex(mm, digest, 16);
}

