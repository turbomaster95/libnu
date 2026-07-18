#include <intnu.h>
#include <nu.h>

bool nu_slab_init(nu_mm_t *mm) {
    mm->slab_sizes[0] = 32;
    mm->slab_sizes[1] = 64;
    mm->slab_sizes[2] = 128;
    mm->slab_sizes[3] = 256;

    mm->slab_region_size = mm->pool_size / 4;
    if (mm->slab_region_size < 32) {
        return false;
    }

    for (int i = 0; i < 4; i++) {
        mm->slab_regions[i] = mm->pool_start + (i * mm->slab_region_size);
        mm->slab_freelist[i] = NULL;

        size_t chunk_size = mm->slab_sizes[i];
        uint8_t *reg_ptr = mm->slab_regions[i];
        uintptr_t reg_pad = (8 - ((uintptr_t)reg_ptr & 7)) & 7;
        reg_ptr += reg_pad;
        size_t usable_size = mm->slab_region_size - reg_pad;

        uint8_t *curr = reg_ptr;
        while (curr + chunk_size <= reg_ptr + usable_size) {
            nu_slab_node_t *node = (nu_slab_node_t *)curr;
            node->next = mm->slab_freelist[i];
            mm->slab_freelist[i] = node;
            curr += chunk_size;
        }
    }
    return true;
}

void* nu_slab_alloc(nu_mm_t *mm, size_t size) {
    int idx = -1;
    for (int i = 0; i < 4; i++) {
        if (size <= mm->slab_sizes[i]) {
            idx = i;
            break;
        }
    }
    if (idx != -1 && mm->slab_freelist[idx]) {
        nu_slab_node_t *node = mm->slab_freelist[idx];
        mm->slab_freelist[idx] = node->next;
        return (void *)node;
    }
    return NULL;
}

void nu_slab_free(nu_mm_t *mm, void *ptr) {
    uint8_t *uptr = (uint8_t *)ptr;
    int idx = -1;
    for (int i = 0; i < 4; i++) {
        if (uptr >= mm->slab_regions[i] && uptr < mm->slab_regions[i] + mm->slab_region_size) {
            idx = i;
            break;
        }
    }
    if (idx != -1) {
        nu_slab_node_t *node = (nu_slab_node_t *)ptr;
        node->next = mm->slab_freelist[idx];
        mm->slab_freelist[idx] = node;
    }
}

size_t nu_slab_get_size(nu_mm_t *mm, void *ptr) {
    uint8_t *uptr = (uint8_t *)ptr;
    for (int i = 0; i < 4; i++) {
        if (uptr >= mm->slab_regions[i] && uptr < mm->slab_regions[i] + mm->slab_region_size) {
            return mm->slab_sizes[i];
        }
    }
    return 0;
}
