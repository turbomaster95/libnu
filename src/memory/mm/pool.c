#include <intnu.h>
#include <nu.h>
#include <stdint.h>
#include <stddef.h>

static size_t pool_align_size(size_t size) {
    size_t align = sizeof(void *);

    if (size < align) {
        size = align;
    }

    if (size > (size_t)-1 - (align - 1)) {
        return 0;
    }

    return (size + align - 1) & ~(align - 1);
}

bool nu_pool_init(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->pool_size < sizeof(void *)) {
        return false;
    }

    mm->pool_object_size = 0;
    mm->pool_object_count = 0;
    mm->pool_free_count = 0;
    mm->pool_freelist = NULL;

    return true;
}

void *nu_pool_alloc(nu_mm_t *mm, size_t size) {
    if (!mm || size == 0) {
        return NULL;
    }

    size_t object_size = pool_align_size(size);

    if (object_size == 0) {
        return NULL;
    }

    if (mm->pool_object_size == 0) {
        size_t count = mm->pool_size / object_size;

        if (count == 0) {
            return NULL;
        }

        mm->pool_object_size = object_size;
        mm->pool_object_count = count;
        mm->pool_free_count = count;
        mm->pool_freelist = mm->pool_start;

        uint8_t *base = mm->pool_start;

        for (size_t i = 0; i < count; i++) {
            void **slot = (void **)(base + i * object_size);

            if (i + 1 < count) {
                *slot = base + (i + 1) * object_size;
            } else {
                *slot = NULL;
            }
        }
    }

    if (object_size != mm->pool_object_size) {
        return NULL;
    }

    if (!mm->pool_freelist || mm->pool_free_count == 0) {
        return NULL;
    }

    void *ptr = mm->pool_freelist;
    mm->pool_freelist = *(void **)ptr;
    mm->pool_free_count--;

    return ptr;
}

void nu_pool_free(nu_mm_t *mm, void *ptr) {
    if (!mm || !ptr || mm->pool_object_size == 0) {
        return;
    }

    uintptr_t start = (uintptr_t)mm->pool_start;
    uintptr_t end = start + mm->pool_object_count * mm->pool_object_size;
    uintptr_t addr = (uintptr_t)ptr;

    if (addr < start || addr >= end) {
        return;
    }

    if ((addr - start) % mm->pool_object_size != 0) {
        return;
    }

    if (mm->pool_free_count >= mm->pool_object_count) {
        return;
    }

    *(void **)ptr = mm->pool_freelist;
    mm->pool_freelist = ptr;
    mm->pool_free_count++;
}

void *nu_pool_realloc(nu_mm_t *mm, void *ptr, size_t size) {
    if (!mm) {
        return NULL;
    }

    if (!ptr) {
        return nu_pool_alloc(mm, size);
    }

    if (size == 0) {
        nu_pool_free(mm, ptr);
        return NULL;
    }

    if (!mm->pool_object_size) {
        return NULL;
    }

    if (pool_align_size(size) <= mm->pool_object_size) {
        return ptr;
    }

    return NULL;
}

void nu_pool_reset(nu_mm_t *mm) {
    if (!mm || !mm->pool_start || mm->pool_object_size == 0) {
        return;
    }

    size_t count = mm->pool_size / mm->pool_object_size;

    if (count == 0) {
        mm->pool_object_count = 0;
        mm->pool_free_count = 0;
        mm->pool_freelist = NULL;
        return;
    }

    mm->pool_object_count = count;
    mm->pool_free_count = count;
    mm->pool_freelist = mm->pool_start;

    uint8_t *base = mm->pool_start;

    for (size_t i = 0; i < count; i++) {
        void **slot = (void **)(base + i * mm->pool_object_size);

        if (i + 1 < count) {
            *slot = base + (i + 1) * mm->pool_object_size;
        } else {
            *slot = NULL;
        }
    }
}
