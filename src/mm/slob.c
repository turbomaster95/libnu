#include <intnu.h>
#include <nu.h>

bool nu_slob_init(nu_mm_t *mm) {
    if (mm->pool_size <= sizeof(nu_slob_node_t)) {
        return false;
    }
    mm->slob_head = (nu_slob_node_t *)mm->pool_start;
    mm->slob_head->size = mm->pool_size - sizeof(nu_slob_node_t);
    mm->slob_head->is_free = true;
    mm->slob_head->next = NULL;
    return true;
}

void* nu_slob_alloc(nu_mm_t *mm, size_t size) {
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
    return NULL;
}

void nu_slob_free(nu_mm_t *mm, void *ptr) {
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
}
