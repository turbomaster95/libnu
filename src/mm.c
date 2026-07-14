#include <nu.h>
#include <stdint.h>
#include <string.h>

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

nu_mm_t* nu_mm_create(nu_mm_type_t type, void *backing_mem, size_t size) {
    if (!backing_mem || size <= sizeof(nu_mm_t)) {
        return NULL;
    }

    nu_mm_t *mm = (nu_mm_t *)backing_mem;
    mm->type = type;
    mm->pool_start = (uint8_t *)backing_mem + sizeof(nu_mm_t);
    mm->pool_size = size - sizeof(nu_mm_t);

    uintptr_t alignment_padding = (8 - ((uintptr_t)mm->pool_start & 7)) & 7;
    if (mm->pool_size <= alignment_padding) {
        return NULL;
    }
    mm->pool_start += alignment_padding;
    mm->pool_size -= alignment_padding;

    if (mm->type == NU_MM_ARENA) {
        mm->arena_offset = 0;
    } else if (mm->type == NU_MM_SLOB) {
        if (mm->pool_size <= sizeof(nu_slob_node_t)) {
            return NULL;
        }
        mm->slob_head = (nu_slob_node_t *)mm->pool_start;
        mm->slob_head->size = mm->pool_size - sizeof(nu_slob_node_t);
        mm->slob_head->is_free = true;
        mm->slob_head->next = NULL;
    } else if (mm->type == NU_MM_SLAB) {
        mm->slab_sizes[0] = 32;
        mm->slab_sizes[1] = 64;
        mm->slab_sizes[2] = 128;
        mm->slab_sizes[3] = 256;

        mm->slab_region_size = mm->pool_size / 4;
        if (mm->slab_region_size < 32) {
            return NULL;
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
    } else if (mm->type == NU_MM_BUDDY) {
        size_t p2 = 1;
        while (p2 * 2 <= mm->pool_size) {
            p2 *= 2;
        }
        if (p2 <= sizeof(nu_buddy_node_t)) {
            return NULL;
        }
        mm->pool_size = p2;
        nu_buddy_node_t *root = (nu_buddy_node_t *)mm->pool_start;
        root->size = mm->pool_size;
        root->is_free = true;
    }

    return mm;
}

void nu_mm_destroy(nu_mm_t *mm) {
    if (mm) {
        mm->pool_start = NULL;
        mm->pool_size = 0;
    }
}

void* nu_alloc(nu_mm_t *mm, size_t size) {
    if (!mm || size == 0) {
        return NULL;
    }

    if (mm->type == NU_MM_ARENA) {
        size = (size + 7) & ~7;
        if (mm->arena_offset + size <= mm->pool_size) {
            void *ptr = mm->pool_start + mm->arena_offset;
            mm->arena_offset += size;
            return ptr;
        }
    } else if (mm->type == NU_MM_SLOB) {
        size = (size + 7) & ~7;
        nu_slob_node_t *curr = mm->slob_head;
        while (curr) {
            if (curr->is_free && curr->size >= size) {
                if (curr->size >= size + sizeof(nu_slob_node_t) + 8) {
                    nu_slob_node_t *next_node = (nu_slob_node_t *)((uint8_t *)curr + sizeof(nu_slob_node_t) + size);
                    next_node->size = curr->size - size - sizeof(nu_slob_node_t);
                    next_node->is_free = true;
                    next_node->next = curr->next;
                    curr->size = size;
                    curr->next = next_node;
                }
                curr->is_free = false;
                return (void *)((uint8_t *)curr + sizeof(nu_slob_node_t));
            }
            curr = curr->next;
        }
    } else if (mm->type == NU_MM_SLAB) {
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
    } else if (mm->type == NU_MM_BUDDY) {
        size_t target_size = 1;
        while (target_size < size + sizeof(nu_buddy_node_t)) {
            target_size *= 2;
        }
        if (target_size < 16) {
            target_size = 16;
        }
        size_t offset = 0;
        nu_buddy_node_t *best_match = NULL;
        while (offset < mm->pool_size) {
            nu_buddy_node_t *node = (nu_buddy_node_t *)(mm->pool_start + offset);
            if (node->is_free && node->size >= target_size) {
                if (!best_match || node->size < best_match->size) {
                    best_match = node;
                }
            }
            offset += node->size;
        }
        if (best_match) {
            while (best_match->size / 2 >= target_size) {
                size_t new_size = best_match->size / 2;
                best_match->size = new_size;
                nu_buddy_node_t *buddy = (nu_buddy_node_t *)((uint8_t *)best_match + new_size);
                buddy->size = new_size;
                buddy->is_free = true;
            }
            best_match->is_free = false;
            return (void *)((uint8_t *)best_match + sizeof(nu_buddy_node_t));
        }
    }

    return NULL;
}

void nu_free(nu_mm_t *mm, void *ptr) {
    if (!mm || !ptr) {
        return;
    }

    if (mm->type == NU_MM_SLOB) {
        nu_slob_node_t *node = (nu_slob_node_t *)((uint8_t *)ptr - sizeof(nu_slob_node_t));
        node->is_free = true;
        nu_slob_node_t *curr = mm->slob_head;
        while (curr) {
            while (curr->is_free && curr->next && curr->next->is_free) {
                curr->size += sizeof(nu_slob_node_t) + curr->next->size;
                curr->next = curr->next->next;
            }
            curr = curr->next;
        }
    } else if (mm->type == NU_MM_SLAB) {
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
    } else if (mm->type == NU_MM_BUDDY) {
        nu_buddy_node_t *node = (nu_buddy_node_t *)((uint8_t *)ptr - sizeof(nu_buddy_node_t));
        node->is_free = true;
        while (node->size < mm->pool_size) {
            size_t block_offset = (uint8_t *)node - mm->pool_start;
            size_t buddy_offset = block_offset ^ node->size;
            if (buddy_offset >= mm->pool_size) {
                break;
            }
            nu_buddy_node_t *buddy = (nu_buddy_node_t *)(mm->pool_start + buddy_offset);
            if (!buddy->is_free || buddy->size != node->size) {
                break;
            }
            if (buddy_offset < block_offset) {
                buddy->size *= 2;
                node = buddy;
            } else {
                node->size *= 2;
            }
        }
    }
}

void* nu_realloc(nu_mm_t *mm, void *ptr, size_t size) {
    if (!mm) {
        return NULL;
    }
    if (!ptr) {
        return nu_alloc(mm, size);
    }
    if (size == 0) {
        nu_free(mm, ptr);
        return NULL;
    }

    // Determine the original size of the allocated block
    size_t old_size = 0;

    if (mm->type == NU_MM_SLOB) {
        nu_slob_node_t *node = (nu_slob_node_t *)((uint8_t *)ptr - sizeof(nu_slob_node_t));
        old_size = node->size;
    } 
    else if (mm->type == NU_MM_BUDDY) {
        nu_buddy_node_t *node = (nu_buddy_node_t *)((uint8_t *)ptr - sizeof(nu_buddy_node_t));
        old_size = node->size - sizeof(nu_buddy_node_t);
    } 
    else if (mm->type == NU_MM_SLAB) {
        uint8_t *uptr = (uint8_t *)ptr;
        int idx = -1;
        for (int i = 0; i < 4; i++) {
            if (uptr >= mm->slab_regions[i] && uptr < mm->slab_regions[i] + mm->slab_region_size) {
                idx = i;
                break;
            }
        }
        if (idx != -1) {
            old_size = mm->slab_sizes[idx];
        } else {
            return NULL; // Pointer wasn't within any valid slab region
        }
    } 
    else if (mm->type == NU_MM_ARENA) {
        size_t aligned_size = (size + 7) & ~7;
        uint8_t *uptr = (uint8_t *)ptr;
        if (uptr + ((mm->arena_offset - (size_t)(uptr - mm->pool_start))) == mm->pool_start + mm->arena_offset) {
            size_t current_alloc_size = (mm->pool_start + mm->arena_offset) - uptr;
            size_t base_offset = uptr - mm->pool_start;
            if (base_offset + aligned_size <= mm->pool_size) {
                mm->arena_offset = base_offset + aligned_size;
                return ptr;
            }
        }
        old_size = size; 
    }

    if (size <= old_size) {
        return ptr;
    }

    // Allocate new block
    void *new_ptr = nu_alloc(mm, size);
    if (!new_ptr) {
        return NULL; // Keep original block intact on allocation failure
    }

    size_t copy_size = (old_size < size) ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);

    nu_free(mm, ptr);

    return new_ptr;
}

