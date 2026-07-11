#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../include/nu.h"

#define ARENA_POOL_SIZE 2048
#define SLOB_POOL_SIZE  4096

static uint8_t arena_backing[ARENA_POOL_SIZE];
static uint8_t slob_backing[SLOB_POOL_SIZE];

int main(void) {
    nu_mm_t *arena_allocator = nu_mm_create(NU_MM_ARENA, arena_backing, ARENA_POOL_SIZE);
    if (!arena_allocator) {
        NU_ERROR("Error: Failed to initialize Arena allocator\n");
        return 1;
    }

    nu_mm_t *slob_allocator = nu_mm_create(NU_MM_SLOB, slob_backing, SLOB_POOL_SIZE);
    if (!slob_allocator) {
        NU_ERROR("Error: Failed to initialize SLOB allocator\n");
        return 1;
    }

    int *scores = (int *)nu_alloc(arena_allocator, 5 * sizeof(int));
    char *msg = (char *)nu_alloc(arena_allocator, 64);

    if (scores && msg) {
        for (int i = 0; i < 5; i++) {
            scores[i] = i * 10;
        }
        strcpy(msg, "Hello from the Arena instance!");
        NU_INFO("Arena data: scores[4] = %d, msg = %s\n", scores[4], msg);
    }

    void *block_a = nu_alloc(slob_allocator, 128);
    void *block_b = nu_alloc(slob_allocator, 512);

    if (block_a && block_b) {
        NU_INFO("SLOB allocations successful at %p and %p\n", block_a, block_b);
    }

    nu_free(slob_allocator, block_a);

    void *block_c = nu_alloc(slob_allocator, 64);
    if (block_c) {
        NU_INFO("SLOB successfully reused fragmented space at %p\n", block_c);
    }

    nu_free(slob_allocator, block_b);
    nu_free(slob_allocator, block_c);

    nu_mm_destroy(arena_allocator);
    nu_mm_destroy(slob_allocator);
    return 0;
}
