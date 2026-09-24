#include <intnu.h>
#include <nu.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static size_t arena_align(size_t size) {
    size_t align = sizeof(void *);

    if (size > (size_t)-1 - (align - 1)) {
        return 0;
    }

    return (size + align - 1) & ~(align - 1);
}

bool nu_arena_init(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->pool_size == 0) {
        return false;
    }

    mm->arena_offset = 0;
    return true;
}

void *nu_arena_alloc(nu_mm_t *mm, size_t size) {
    if (!mm || size == 0) {
        return NULL;
    }

    size_t header_size = arena_align(sizeof(nu_arena_node_t));
    size_t data_size = arena_align(size);

    if (header_size == 0 || data_size == 0) {
        return NULL;
    }

    if (header_size > mm->pool_size - mm->arena_offset) {
        return NULL;
    }

    size_t remaining = mm->pool_size - mm->arena_offset - header_size;

    if (data_size > remaining) {
        return NULL;
    }

    uint8_t *base = mm->pool_start + mm->arena_offset;
    nu_arena_node_t *node = (nu_arena_node_t *)base;

    node->size = size;
    mm->arena_offset += header_size + data_size;

    return base + header_size;
}

void *nu_arena_realloc(nu_mm_t *mm, void *ptr, size_t size) {
    if (!mm) {
        return NULL;
    }

    if (!ptr) {
        return nu_arena_alloc(mm, size);
    }

    if (size == 0) {
        return NULL;
    }

    size_t header_size = arena_align(sizeof(nu_arena_node_t));
    uint8_t *base = (uint8_t *)ptr - header_size;

    uintptr_t start = (uintptr_t)mm->pool_start;
    uintptr_t end = start + mm->arena_offset;
    uintptr_t addr = (uintptr_t)ptr;

    if (addr < start + header_size || addr >= end) {
        return NULL;
    }

    nu_arena_node_t *node = (nu_arena_node_t *)base;
    size_t old_size = node->size;
    size_t old_data_size = arena_align(old_size);
    size_t new_data_size = arena_align(size);

    if (old_data_size == 0 || new_data_size == 0) {
        return NULL;
    }

    uintptr_t allocation_end = (uintptr_t)base + header_size + old_data_size;

    if (allocation_end != start + mm->arena_offset) {
        void *new_ptr = nu_arena_alloc(mm, size);

        if (!new_ptr) {
            return NULL;
        }

        size_t copy_size = old_size < size ? old_size : size;
        memcpy(new_ptr, ptr, copy_size);

        return new_ptr;
    }

    size_t prefix = (size_t)((uintptr_t)base - start);

    if (prefix > mm->pool_size) {
        return NULL;
    }

    if (header_size > mm->pool_size - prefix) {
        return NULL;
    }

    if (new_data_size > mm->pool_size - prefix - header_size) {
        return NULL;
    }

    mm->arena_offset = prefix + header_size + new_data_size;
    node->size = size;

    return ptr;
}

void nu_arena_reset(nu_mm_t *mm) {
    if (!mm) {
        return;
    }

    mm->arena_offset = 0;
}
