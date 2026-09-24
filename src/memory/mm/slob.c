#include <intnu.h>
#include <nu.h>
#include <stdint.h>
#include <stddef.h>

static size_t slob_align(size_t size) {
    size_t align = sizeof(void *);

    if (size > (size_t)-1 - (align - 1)) {
        return 0;
    }

    return (size + align - 1) & ~(align - 1);
}

static bool slob_ptr_valid(nu_mm_t *mm, void *ptr) {
    if (!mm || !ptr || !mm->pool_start) {
        return false;
    }

    uintptr_t start = (uintptr_t)mm->pool_start;
    uintptr_t end = start + mm->pool_size;
    uintptr_t addr = (uintptr_t)ptr;

    return addr >= start + sizeof(nu_slob_node_t) && addr < end;
}

bool nu_slob_init(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->pool_size <= sizeof(nu_slob_node_t)) {
        return false;
    }

    nu_slob_reset(mm);
    return true;
}

void *nu_slob_alloc(nu_mm_t *mm, size_t size) {
    if (!mm || size == 0) {
        return NULL;
    }

    size = slob_align(size);

    if (size == 0) {
        return NULL;
    }

    nu_slob_node_t *curr = mm->slob_head;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            size_t minimum_split = sizeof(nu_slob_node_t) + sizeof(void *);

            if (curr->size >= size && curr->size - size >= minimum_split) {
                uint8_t *next_addr = (uint8_t *)curr + sizeof(nu_slob_node_t) + size;
                nu_slob_node_t *next = (nu_slob_node_t *)next_addr;

                next->size = curr->size - size - sizeof(nu_slob_node_t);
                next->is_free = true;
                next->next = curr->next;

                curr->size = size;
                curr->next = next;
            }

            curr->is_free = false;
            return (uint8_t *)curr + sizeof(nu_slob_node_t);
        }

        curr = curr->next;
    }

    return NULL;
}

void nu_slob_free(nu_mm_t *mm, void *ptr) {
    if (!slob_ptr_valid(mm, ptr)) {
        return;
    }

    nu_slob_node_t *node = (nu_slob_node_t *)((uint8_t *)ptr - sizeof(nu_slob_node_t));

    if (node->is_free) {
        return;
    }

    node->is_free = true;

    nu_slob_node_t *curr = mm->slob_head;

    while (curr) {
        if (curr->is_free && curr->next && curr->next->is_free) {
            curr->size += sizeof(nu_slob_node_t) + curr->next->size;
            curr->next = curr->next->next;
            continue;
        }

        curr = curr->next;
    }
}

void nu_slob_reset(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->pool_size <= sizeof(nu_slob_node_t)) {
        return;
    }

    mm->slob_head = (nu_slob_node_t *)mm->pool_start;
    mm->slob_head->size = mm->pool_size - sizeof(nu_slob_node_t);
    mm->slob_head->is_free = true;
    mm->slob_head->next = NULL;
}
