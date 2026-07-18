#ifndef NU_INTERNAL_H
#define NU_INTERNAL_H

#include <nu.h>
#include <stdbool.h>

#define MAX_EVENTS 64

#define ROL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n)   ((x) >> (n))

typedef struct nu_slob_node {
    size_t size;
    bool is_free;
    struct nu_slob_node *next;
} nu_slob_node_t;

typedef struct nu_slab_node {
    struct nu_slab_node *next;
} nu_slab_node_t;

typedef struct nu_buddy_node {
    size_t size;
    bool is_free;
} nu_buddy_node_t;

struct nu_mm {
    nu_mm_type_t type;
    uint8_t *pool_start;
    size_t pool_size;
    size_t arena_offset;
    nu_slob_node_t *slob_head;
    nu_slab_node_t *slab_freelist[4];
    size_t slab_sizes[4];
    uint8_t *slab_regions[4];
    size_t slab_region_size;
};

// Internal strategy hooks
bool nu_arena_init(nu_mm_t *mm);
void* nu_arena_alloc(nu_mm_t *mm, size_t size);
void* nu_arena_realloc(nu_mm_t *mm, void *ptr, size_t size);

bool nu_slob_init(nu_mm_t *mm);
void* nu_slob_alloc(nu_mm_t *mm, size_t size);
void nu_slob_free(nu_mm_t *mm, void *ptr);

bool nu_slab_init(nu_mm_t *mm);
void* nu_slab_alloc(nu_mm_t *mm, size_t size);
void nu_slab_free(nu_mm_t *mm, void *ptr);
size_t nu_slab_get_size(nu_mm_t *mm, void *ptr);

bool nu_buddy_init(nu_mm_t *mm);
void* nu_buddy_alloc(nu_mm_t *mm, size_t size);
void nu_buddy_free(nu_mm_t *mm, void *ptr);

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

