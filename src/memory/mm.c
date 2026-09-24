#include <intnu.h>
#include <nu.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

nu_mm_t *nu_mm_create(nu_mm_type_t type, void *backing_mem, size_t size) {
    if (!backing_mem || size <= sizeof(nu_mm_t)) {
        return NULL;
    }

    uintptr_t raw = (uintptr_t)backing_mem;
    nu_mm_t *mm = (nu_mm_t *)backing_mem;

    memset(mm, 0, sizeof(*mm));
    mm->type = type;

    uintptr_t pool_addr = raw + sizeof(nu_mm_t);
    size_t available = size - sizeof(nu_mm_t);
    uintptr_t aligned_addr = (pool_addr + 7u) & ~(uintptr_t)7u;
    size_t padding = (size_t)(aligned_addr - pool_addr);

    if (padding >= available) {
        return NULL;
    }

    mm->pool_start = (uint8_t *)aligned_addr;
    mm->pool_size = available - padding;

    bool ok = false;

    switch (type) {
        case NU_MM_ARENA: ok = nu_arena_init(mm); break;
        case NU_MM_SLOB: ok = nu_slob_init(mm); break;
        case NU_MM_SLAB: ok = nu_slab_init(mm); break;
        case NU_MM_BUDDY: ok = nu_buddy_init(mm); break;
        case NU_MM_POOL: ok = nu_pool_init(mm); break;
        case NU_MM_STACK: ok = nu_stack_init(mm); break;
        default: return NULL;
    }

    if (!ok) {
        mm->pool_start = NULL;
        mm->pool_size = 0;
        return NULL;
    }

    return mm;
}

void *nu_alloc(nu_mm_t *mm, size_t size) {
    if (!mm || !mm->pool_start || size == 0) {
        return NULL;
    }

    switch (mm->type) {
        case NU_MM_ARENA: return nu_arena_alloc(mm, size);
        case NU_MM_SLOB: return nu_slob_alloc(mm, size);
        case NU_MM_SLAB: return nu_slab_alloc(mm, size);
        case NU_MM_BUDDY: return nu_buddy_alloc(mm, size);
        case NU_MM_POOL: return nu_pool_alloc(mm, size);
        case NU_MM_STACK: return nu_stack_alloc(mm, size);
        default: return NULL;
    }
}

void nu_free(nu_mm_t *mm, void *ptr) {
    if (!mm || !ptr || !mm->pool_start) {
        return;
    }

    switch (mm->type) {
        case NU_MM_SLOB: nu_slob_free(mm, ptr); break;
        case NU_MM_BUDDY: nu_buddy_free(mm, ptr); break;
        case NU_MM_SLAB: nu_slab_free(mm, ptr); break;
        case NU_MM_POOL: nu_pool_free(mm, ptr); break;
        case NU_MM_STACK: nu_stack_free(mm, ptr); break;
        case NU_MM_ARENA: break;
        default: break;
    }
}

void *nu_realloc(nu_mm_t *mm, void *ptr, size_t size) {
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

    switch (mm->type) {
        case NU_MM_ARENA: return nu_arena_realloc(mm, ptr, size);
        case NU_MM_POOL: return nu_pool_realloc(mm, ptr, size);
        case NU_MM_STACK: return nu_stack_realloc(mm, ptr, size);
        default: break;
    }

    size_t old_size = 0;

    if (mm->type == NU_MM_SLOB) {
        uintptr_t addr = (uintptr_t)ptr;
        uintptr_t start = (uintptr_t)mm->pool_start;
        uintptr_t end = start + mm->pool_size;

        if (addr < start + sizeof(nu_slob_node_t) || addr >= end) {
            return NULL;
        }

        nu_slob_node_t *node = (nu_slob_node_t *)((uint8_t *)ptr - sizeof(nu_slob_node_t));

        if (node->is_free || node->size > mm->pool_size) {
            return NULL;
        }

        old_size = node->size;
    } else if (mm->type == NU_MM_BUDDY) {
        uintptr_t addr = (uintptr_t)ptr;
        uintptr_t start = (uintptr_t)mm->pool_start;
        uintptr_t end = start + mm->pool_size;

        if (addr < start + sizeof(nu_buddy_node_t) || addr >= end) {
            return NULL;
        }

        nu_buddy_node_t *node = (nu_buddy_node_t *)((uint8_t *)ptr - sizeof(nu_buddy_node_t));

        if (node->is_free || node->size < sizeof(nu_buddy_node_t) || node->size > mm->pool_size) {
            return NULL;
        }

        old_size = node->size - sizeof(nu_buddy_node_t);
    } else if (mm->type == NU_MM_SLAB) {
        old_size = nu_slab_get_size(mm, ptr);

        if (old_size == 0) {
            return NULL;
        }
    }

    if (size <= old_size) {
        return ptr;
    }

    void *new_ptr = nu_alloc(mm, size);

    if (!new_ptr) {
        return NULL;
    }

    memcpy(new_ptr, ptr, old_size);
    nu_free(mm, ptr);

    return new_ptr;
}

void nu_mm_destroy(nu_mm_t *mm) {
    if (!mm) {
        return;
    }

    mm->pool_start = NULL;
    mm->pool_size = 0;
    mm->arena_offset = 0;
    mm->slob_head = NULL;
    mm->pool_freelist = NULL;
    mm->stack_offset = 0;

    for (int i = 0; i < 4; i++) {
        mm->slab_freelist[i] = NULL;
        mm->slab_regions[i] = NULL;
        mm->slab_sizes[i] = 0;
    }
}

void nu_mm_reset(nu_mm_t *mm) {
    if (!mm || !mm->pool_start) {
        return;
    }

    switch (mm->type) {
        case NU_MM_ARENA: nu_arena_reset(mm); break;
        case NU_MM_SLOB: nu_slob_reset(mm); break;
        case NU_MM_SLAB: nu_slab_reset(mm); break;
        case NU_MM_BUDDY: nu_buddy_reset(mm); break;
        case NU_MM_POOL: nu_pool_reset(mm); break;
        case NU_MM_STACK: nu_stack_reset(mm); break;
        default: break;
    }
}
