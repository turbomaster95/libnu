#include <nu.h>

#define NU_LZ4_HASH_BITS 12
#define NU_LZ4_HASH_SIZE (1 << NU_LZ4_HASH_BITS)
#define NU_LZ4_MIN_MATCH 4

static inline uint32_t nu_lz4_hash(uint32_t val) {
    return (val * 2654435761U) >> (32 - NU_LZ4_HASH_BITS);
}

static inline uint32_t nu_lz4_read32(const uint8_t *ptr) {
    return (uint32_t)ptr[0]        | ((uint32_t)ptr[1] << 8) | 
           ((uint32_t)ptr[2] << 16) | ((uint32_t)ptr[3] << 24);
}

size_t nu_lz4_compress_bound(size_t src_size) {
    return src_size + (src_size / 255) + 16;
}

size_t nu_lz4_compress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_max_size) {
    if (dst_max_size < nu_lz4_compress_bound(src_size)) return 0;

    const uint8_t *hash_table[NU_LZ4_HASH_SIZE] = { NULL };

    const uint8_t *ip = src;
    const uint8_t *const ip_end = src + src_size;
    const uint8_t *const ip_limit = ip_end - 5; // Prevent out-of-bounds scanning at tail
    const uint8_t *anchor = src;

    uint8_t *op = dst;
    uint8_t *const op_end = dst + dst_max_size;

    while (ip < ip_limit) {
        uint32_t hv = nu_lz4_hash(nu_lz4_read32(ip));
        const uint8_t *match = hash_table[hv];
        hash_table[hv] = ip;

        if (match && (ip - match < 65536) && (nu_lz4_read32(match) == nu_lz4_read32(ip))) {
            size_t lit_len = (size_t)(ip - anchor);
            size_t match_len = 0;

            ip += NU_LZ4_MIN_MATCH;
            match += NU_LZ4_MIN_MATCH;
            while (ip < ip_end && *match == *ip) {
                skip: ip++; match++; match_len++;
            }

            uint8_t *token = op++;
            if (op >= op_end) return 0;
            *token = 0;

            if (lit_len >= 15) {
                *token |= (15 << 4);
                size_t len = lit_len - 15;
                while (len >= 255) {
                    *op++ = 255; len -= 255;
                    if (op >= op_end) return 0;
                }
                *op++ = (uint8_t)len;
            } else {
                *token |= (lit_len << 4);
            }

            for (size_t i = 0; i < lit_len; i++) {
                *op++ = anchor[i];
                if (op >= op_end) return 0;
            }

            uint16_t offset = (uint16_t)(ip - match_len - NU_LZ4_MIN_MATCH - anchor - lit_len);
            *op++ = (uint8_t)(offset & 0xFF);
            *op++ = (uint8_t)((offset >> 8) & 0xFF);
            if (op >= op_end) return 0;

            if (match_len >= 15) {
                *token |= 15;
                size_t len = match_len - 15;
                while (len >= 255) {
                    *op++ = 255; len -= 255;
                    if (op >= op_end) return 0;
                }
                *op++ = (uint8_t)len;
            } else {
                *token |= match_len;
            }

            anchor = ip;
            continue;
        }
        ip++;
    }

    size_t last_lit_len = (size_t)(ip_end - anchor);
    uint8_t *token = op++;
    if (op >= op_end) return 0;
    *token = 0;

    if (last_lit_len >= 15) {
        *token |= (15 << 4);
        size_t len = last_lit_len - 15;
        while (len >= 255) {
            *op++ = 255; len -= 255;
            if (op >= op_end) return 0;
        }
        *op++ = (uint8_t)len;
    } else {
        *token |= (last_lit_len << 4);
    }

    for (size_t i = 0; i < last_lit_len; i++) {
        *op++ = anchor[i];
        if (op >= op_end) return 0;
    }

    return (size_t)(op - dst);
}

size_t nu_lz4_decompress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_max_size) {
    const uint8_t *ip = src;
    const uint8_t *const ip_end = src + src_size;

    uint8_t *op = dst;
    uint8_t *const op_end = dst + dst_max_size;

    while (ip < ip_end) {
        uint8_t token = *ip++;

        size_t lit_len = (token >> 4);
        if (lit_len == 15) {
            uint8_t s;
            do {
                s = *ip++;
                lit_len += s;
            } while (s == 255 && ip < ip_end);
        }

        if (op + lit_len > op_end || ip + lit_len > ip_end) return 0;
        for (size_t i = 0; i < lit_len; i++) {
            *op++ = *ip++;
        }

        if (ip >= ip_end) break; // Trailing literals reached end of sequence cleanly

        if (ip + 2 > ip_end) return 0;
        uint16_t offset = (uint16_t)ip[0] | ((uint16_t)ip[1] << 8);
        ip += 2;

        const uint8_t *match = op - offset;
        if (match < dst) return 0; // Guard against malicious back-reference pointer underflows

        size_t match_len = (token & 0x0F);
        if (match_len == 15) {
            uint8_t s;
            do {
                s = *ip++;
                match_len += s;
            } while (s == 255 && ip < ip_end);
        }
        match_len += NU_LZ4_MIN_MATCH; // Add baseline match length requirements

        if (op + match_len > op_end) return 0;
        for (size_t i = 0; i < match_len; i++) {
            *op++ = *match++;
        }
    }

    return (size_t)(op - dst);
}

