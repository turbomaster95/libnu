#include <intnu.h>
#include <nu.h>
#include <stdint.h>
#include <stddef.h>

bool nu_slab_init(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->pool_size < 128) {
        return false;
    }

    mm->slab_sizes[0] = 32;
    mm->slab_sizes[1] = 64;
    mm->slab_sizes[2] = 128;
    mm->slab_sizes[3] = 256;

    mm->slab_region_size = mm->pool_size / 4;

    if (mm->slab_region_size < 32) {
        return false;
    }

    nu_slab_reset(mm);

    return true;
}

void *nu_slab_alloc(nu_mm_t *mm, size_t size) {
    if (!mm || size == 0) {
        return NULL;
    }

    int idx = -1;

    for (int i = 0; i < 4; i++) {
        if (size <= mm->slab_sizes[i]) {
            idx = i;
            break;
        }
    }

    if (idx < 0 || !mm->slab_freelist[idx]) {
        return NULL;
    }

    nu_slab_node_t *node = mm->slab_freelist[idx];
    mm->slab_freelist[idx] = node->next;

    return node;
}

static int nu_slab_find_chunk(nu_mm_t *mm, void *ptr) {
    if (!mm || !ptr) {
        return -1;
    }

    uintptr_t addr = (uintptr_t)ptr;

    for (int i = 0; i < 4; i++) {
        uintptr_t start = (uintptr_t)mm->slab_regions[i];
        uintptr_t end = start + mm->slab_region_size;

        if (addr >= start && addr < end) {
            size_t align_pad = (8 - (start & 7)) & 7;

            if (addr < start + align_pad) {
                return -1;
            }

            size_t offset = (size_t)(addr - (start + align_pad));

            if (offset % mm->slab_sizes[i] != 0) {
                return -1;
            }

            return i;
        }
    }

    return -1;
}

void nu_slab_free(nu_mm_t *mm, void *ptr) {
    int idx = nu_slab_find_chunk(mm, ptr);

    if (idx < 0) {
        return;
    }

    nu_slab_node_t *node = (nu_slab_node_t *)ptr;
    node->next = mm->slab_freelist[idx];
    mm->slab_freelist[idx] = node;
}

size_t nu_slab_get_size(nu_mm_t *mm, void *ptr) {
    int idx = nu_slab_find_chunk(mm, ptr);

    if (idx < 0) {
        return 0;
    }

    return mm->slab_sizes[idx];
}

void nu_slab_reset(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->slab_region_size == 0) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        mm->slab_regions[i] = mm->pool_start + (i * mm->slab_region_size);
        mm->slab_freelist[i] = NULL;

        size_t chunk_size = mm->slab_sizes[i];
        uintptr_t start_addr = (uintptr_t)mm->slab_regions[i];
        size_t padding = (8 - (start_addr & 7)) & 7;

        if (padding >= mm->slab_region_size) {
            continue;
        }

        uint8_t *start = mm->slab_regions[i] + padding;
        size_t usable = mm->slab_region_size - padding;
        size_t count = usable / chunk_size;

        for (size_t j = 0; j < count; j++) {
            nu_slab_node_t *node = (nu_slab_node_t *)(start + j * chunk_size);
            node->next = mm->slab_freelist[i];
            mm->slab_freelist[i] = node;
        }
    }
}
