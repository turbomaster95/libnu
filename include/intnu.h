#ifndef NU_INTERNAL_H
#define NU_INTERNAL_H

#include <nu.h>
#include <stdbool.h>

#define MAX_EVENTS 64

#define ROL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n)   ((x) >> (n))

static char* nu_digest_to_hex(nu_mm_t *mm, const uint8_t *digest, size_t len) {
    char *hex = (char *)nu_alloc(mm, (len * 2) + 1);
    if (!hex) return NULL;

    const char lookup[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        hex[i * 2]     = lookup[(digest[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = lookup[digest[i] & 0x0F];
    }
    hex[len * 2] = '\0';
    return hex;
}

#endif // NU_INTERNAL_H

