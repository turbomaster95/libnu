#include <intnu.h>
#include <nu.h>
#include <string.h>

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

    bool init_success = false;
    switch (mm->type) {
        case NU_MM_ARENA: init_success = nu_arena_init(mm); break;
        case NU_MM_SLOB:  init_success = nu_slob_init(mm);  break;
        case NU_MM_SLAB:  init_success = nu_slab_init(mm);  break;
        case NU_MM_BUDDY: init_success = nu_buddy_init(mm); break;
        default: break;
    }

    if (!init_success) {
        return NULL;
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

    switch (mm->type) {
        case NU_MM_ARENA: return nu_arena_alloc(mm, size);
        case NU_MM_SLOB:  return nu_slob_alloc(mm, size);
        case NU_MM_SLAB:  return nu_slab_alloc(mm, size);
        case NU_MM_BUDDY: return nu_buddy_alloc(mm, size);
        default:          return NULL;
    }
}

void nu_free(nu_mm_t *mm, void *ptr) {
    if (!mm || !ptr) {
        return;
    }

    switch (mm->type) {
        case NU_MM_SLOB:  nu_slob_free(mm, ptr);  break;
        case NU_MM_SLAB:  nu_slab_free(mm, ptr);  break;
        case NU_MM_BUDDY: nu_buddy_free(mm, ptr); break;
        case NU_MM_ARENA: /* Arenas naturally drop allocations all at once */ break;
        default: break;
    }
}

void* nu_realloc(nu_mm_t *mm, void *ptr, size_t size) {
    if (!mm) return NULL;
    if (!ptr) return nu_alloc(mm, size);
    if (size == 0) {
        nu_free(mm, ptr);
        return NULL;
    }

    if (mm->type == NU_MM_ARENA) {
        return nu_arena_realloc(mm, ptr, size);
    }

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
        old_size = nu_slab_get_size(mm, ptr);
        if (old_size == 0) return NULL;
    }

    if (size <= old_size) {
        return ptr;
    }

    void *new_ptr = nu_alloc(mm, size);
    if (!new_ptr) {
        return NULL; 
    }

    size_t copy_size = (old_size < size) ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);
    nu_free(mm, ptr);

    return new_ptr;
}
