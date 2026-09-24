#include <intnu.h>
#include <nu.h>

bool nu_buddy_init(nu_mm_t *mm) {
    size_t p2 = 1;
    while (p2 * 2 <= mm->pool_size) {
        p2 *= 2;
    }
    if (p2 <= sizeof(nu_buddy_node_t)) {
        return false;
    }
    mm->pool_size = p2;
    nu_buddy_node_t *root = (nu_buddy_node_t *)mm->pool_start;
    root->size = mm->pool_size;
    root->is_free = true;
    return true;
}

void* nu_buddy_alloc(nu_mm_t *mm, size_t size) {
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
    return NULL;
}

void nu_buddy_free(nu_mm_t *mm, void *ptr) {
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
