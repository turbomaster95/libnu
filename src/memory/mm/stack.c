#include <intnu.h>
#include <nu.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static size_t stack_align(size_t size) {
    size_t align = sizeof(void *);

    if (size > (size_t)-1 - (align - 1)) {
        return 0;
    }

    return (size + align - 1) & ~(align - 1);
}

bool nu_stack_init(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->pool_size < sizeof(nu_stack_node_t)) {
        return false;
    }

    mm->stack_offset = 0;

    return true;
}

void *nu_stack_alloc(nu_mm_t *mm, size_t size) {
    if (!mm || size == 0) {
        return NULL;
    }

    size_t header_size = stack_align(sizeof(nu_stack_node_t));
    size_t data_size = stack_align(size);

    if (header_size == 0 || data_size == 0) {
        return NULL;
    }

    if (header_size > mm->pool_size - mm->stack_offset) {
        return NULL;
    }

    size_t remaining = mm->pool_size - mm->stack_offset - header_size;

    if (data_size > remaining) {
        return NULL;
    }

    uint8_t *base = mm->pool_start + mm->stack_offset;
    nu_stack_node_t *node = (nu_stack_node_t *)base;

    node->size = size;

    mm->stack_offset += header_size + data_size;

    return base + header_size;
}

void nu_stack_free(nu_mm_t *mm, void *ptr) {
    if (!mm || !ptr || mm->stack_offset == 0) {
        return;
    }

    size_t header_size = stack_align(sizeof(nu_stack_node_t));

    uintptr_t start = (uintptr_t)mm->pool_start;
    uintptr_t stack_end = start + mm->stack_offset;
    uintptr_t addr = (uintptr_t)ptr;

    if (addr < start + header_size || addr >= stack_end) {
        return;
    }

    uint8_t *base = (uint8_t *)ptr - header_size;
    nu_stack_node_t *node = (nu_stack_node_t *)base;

    size_t data_size = stack_align(node->size);

    if (data_size == 0) {
        return;
    }

    uintptr_t allocation_end = (uintptr_t)base + header_size + data_size;

    if (allocation_end != stack_end) {
        return;
    }

    mm->stack_offset -= header_size + data_size;
}

void *nu_stack_realloc(nu_mm_t *mm, void *ptr, size_t size) {
    if (!mm) {
        return NULL;
    }

    if (!ptr) {
        return nu_stack_alloc(mm, size);
    }

    if (size == 0) {
        nu_stack_free(mm, ptr);
        return NULL;
    }

    size_t header_size = stack_align(sizeof(nu_stack_node_t));

    if (header_size == 0) {
        return NULL;
    }

    uint8_t *base = (uint8_t *)ptr - header_size;
    nu_stack_node_t *node = (nu_stack_node_t *)base;

    size_t old_size = node->size;
    size_t old_data_size = stack_align(old_size);
    size_t new_data_size = stack_align(size);

    if (old_data_size == 0 || new_data_size == 0) {
        return NULL;
    }

    uintptr_t start = (uintptr_t)mm->pool_start;
    uintptr_t stack_end = start + mm->stack_offset;
    uintptr_t allocation_end = (uintptr_t)base + header_size + old_data_size;

    if (allocation_end == stack_end) {
        size_t base_offset = (size_t)((uintptr_t)base - start);

        if (base_offset > mm->pool_size) {
            return NULL;
        }

        if (header_size > mm->pool_size - base_offset) {
            return NULL;
        }

        if (new_data_size <= mm->pool_size - base_offset - header_size) {
            mm->stack_offset = base_offset + header_size + new_data_size;
            node->size = size;
            return ptr;
        }
    }

    void *new_ptr = nu_stack_alloc(mm, size);

    if (!new_ptr) {
        return NULL;
    }

    size_t copy_size = old_size < size ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);

    return new_ptr;
}

void nu_stack_reset(nu_mm_t *mm) {
    if (!mm) {
        return;
    }

    mm->stack_offset = 0;
}
