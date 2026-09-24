#include <intnu.h>
#include <nu.h>
#include <stdint.h>
#include <stddef.h>

static bool buddy_ptr_valid(nu_mm_t *mm, void *ptr, nu_buddy_node_t **out_node) {
    if (!mm || !ptr || !mm->pool_start) {
        return false;
    }

    uintptr_t start = (uintptr_t)mm->pool_start;
    uintptr_t end = start + mm->pool_size;
    uintptr_t addr = (uintptr_t)ptr;

    if (addr < start + sizeof(nu_buddy_node_t) || addr >= end) {
        return false;
    }

    nu_buddy_node_t *node = (nu_buddy_node_t *)((uint8_t *)ptr - sizeof(nu_buddy_node_t));

    if (node->size < sizeof(nu_buddy_node_t) || node->size > mm->pool_size) {
        return false;
    }

    uintptr_t node_addr = (uintptr_t)node;

    if (node_addr < start || node_addr >= end) {
        return false;
    }

    if ((node_addr - start) % node->size != 0) {
        return false;
    }

    if (out_node) {
        *out_node = node;
    }

    return true;
}

bool nu_buddy_init(nu_mm_t *mm) {
    if (!mm || !mm->pool_start) {
        return false;
    }

    size_t p2 = 1;

    while (p2 <= mm->pool_size / 2) {
        p2 *= 2;
    }

    if (p2 <= sizeof(nu_buddy_node_t)) {
        return false;
    }

    mm->pool_size = p2;

    nu_buddy_reset(mm);

    return true;
}

void *nu_buddy_alloc(nu_mm_t *mm, size_t size) {
    if (!mm || size == 0) {
        return NULL;
    }

    if (size > (size_t)-1 - sizeof(nu_buddy_node_t)) {
        return NULL;
    }

    size_t required = size + sizeof(nu_buddy_node_t);
    size_t target = 16;

    while (target < required) {
        if (target > (size_t)-1 / 2) {
            return NULL;
        }

        target *= 2;
    }

    if (target > mm->pool_size) {
        return NULL;
    }

    size_t offset = 0;
    nu_buddy_node_t *best = NULL;

    while (offset < mm->pool_size) {
        nu_buddy_node_t *node = (nu_buddy_node_t *)(mm->pool_start + offset);

        if (node->size == 0 || offset > mm->pool_size - node->size) {
            return NULL;
        }

        if (node->is_free && node->size >= target) {
            if (!best || node->size < best->size) {
                best = node;
            }
        }

        offset += node->size;
    }

    if (!best) {
        return NULL;
    }

    while (best->size / 2 >= target) {
        size_t half = best->size / 2;
        nu_buddy_node_t *buddy = (nu_buddy_node_t *)((uint8_t *)best + half);

        best->size = half;
        best->is_free = false;

        buddy->size = half;
        buddy->is_free = true;

        best->is_free = true;
    }

    best->is_free = false;

    return (uint8_t *)best + sizeof(nu_buddy_node_t);
}

void nu_buddy_free(nu_mm_t *mm, void *ptr) {
    nu_buddy_node_t *node = NULL;

    if (!buddy_ptr_valid(mm, ptr, &node)) {
        return;
    }

    if (node->is_free) {
        return;
    }

    node->is_free = true;

    while (node->size < mm->pool_size) {
        uintptr_t node_addr = (uintptr_t)node;
        uintptr_t start = (uintptr_t)mm->pool_start;
        size_t offset = (size_t)(node_addr - start);
        size_t buddy_offset = offset ^ node->size;

        if (buddy_offset >= mm->pool_size) {
            break;
        }

        nu_buddy_node_t *buddy = (nu_buddy_node_t *)(mm->pool_start + buddy_offset);

        if (!buddy->is_free || buddy->size != node->size) {
            break;
        }

        if (buddy_offset < offset) {
            buddy->size *= 2;
            node = buddy;
        } else {
            node->size *= 2;
        }

        node->is_free = true;
    }
}

void nu_buddy_reset(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->pool_size <= sizeof(nu_buddy_node_t)) {
        return;
    }

    nu_buddy_node_t *root = (nu_buddy_node_t *)mm->pool_start;

    root->size = mm->pool_size;
    root->is_free = true;
}
