#include <nu.h>

char* nu_internal_md5(nu_mm_t *mm, const uint8_t *data, size_t len);
char* nu_internal_sha1(nu_mm_t *mm, const uint8_t *data, size_t len);
char* nu_internal_sha256(nu_mm_t *mm, const uint8_t *data, size_t len);

char* nu_hash_encode(nu_mm_t *mm, nu_hash_type_t type, const uint8_t *data, size_t len) {
    if (!mm || !data) return NULL;

    switch (type) {
        case NU_HASH_MD5:    return nu_internal_md5(mm, data, len);
        case NU_HASH_SHA1:   return nu_internal_sha1(mm, data, len);
        case NU_HASH_SHA256: return nu_internal_sha256(mm, data, len);
        default:             return NULL; /* Silently discard unrecognized/unsupported modes */
    }
}

