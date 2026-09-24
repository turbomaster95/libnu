#include <intnu.h>
#include <nu.h>
#include <string.h>

bool nu_arena_init(nu_mm_t *mm) {
    mm->arena_offset = 0;
    return true;
}

void* nu_arena_alloc(nu_mm_t *mm, size_t size) {
    size = (size + 7) & ~7;
    if (mm->arena_offset + size <= mm->pool_size) {
        void *ptr = mm->pool_start + mm->arena_offset;
        mm->arena_offset += size;
        return ptr;
    }
    return NULL;
}

void* nu_arena_realloc(nu_mm_t *mm, void *ptr, size_t size) {
    size_t aligned_size = (size + 7) & ~7;
    uint8_t *uptr = (uint8_t *)ptr;
    
    if (uptr + ((mm->arena_offset - (size_t)(uptr - mm->pool_start))) == mm->pool_start + mm->arena_offset) {
        size_t base_offset = uptr - mm->pool_start;
        if (base_offset + aligned_size <= mm->pool_size) {
            mm->arena_offset = base_offset + aligned_size;
            return ptr;
        }
    }
    
    void *new_ptr = nu_alloc(mm, size);
    if (!new_ptr) return NULL;
    
    memcpy(new_ptr, ptr, size);
    return new_ptr;
}
