#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <nu.h>

#define BACKING_POOL_SIZE 16384
static uint8_t memory_pool[BACKING_POOL_SIZE];

int main(void) {
    nu_mm_t *mm = nu_mm_create(NU_MM_SLOB, memory_pool, BACKING_POOL_SIZE);
    assert(mm != NULL);

    nu_map_t *map = nu_map_create(mm, 8);
    assert(map != NULL);
    assert(nu_map_size(map) == 0);

    int val1 = 42;
    int val2 = 100;
    int val3 = 999;

    assert(nu_map_set(map, "username", &val1) == true);
    assert(nu_map_set(map, "port", &val2) == true);
    assert(nu_map_size(map) == 2);

    int *p_val1 = (int *)nu_map_get(map, "username");
    int *p_val2 = (int *)nu_map_get(map, "port");
    
    assert(p_val1 != NULL && *p_val1 == 42);
    assert(p_val2 != NULL && *p_val2 == 100);

    assert(nu_map_set(map, "username", &val3) == true);
    assert(nu_map_size(map) == 2);
    
    int *p_val3 = (int *)nu_map_get(map, "username");
    assert(p_val3 != NULL && *p_val3 == 999);

    assert(nu_map_get(map, "nonexistent") == NULL);

    assert(nu_map_set(map, "k1", &val1) == true);
    assert(nu_map_set(map, "k2", &val1) == true);
    assert(nu_map_set(map, "k3", &val1) == true);
    assert(nu_map_set(map, "k4", &val1) == true);
    assert(nu_map_set(map, "k5", &val1) == true);
    assert(nu_map_set(map, "k6", &val1) == true);
    assert(nu_map_set(map, "k7", &val1) == true);
    
    assert(nu_map_size(map) == 9);
    assert(*(int *)nu_map_get(map, "port") == 100);

    assert(nu_map_delete(map, "port") == true);
    assert(nu_map_size(map) == 8);
    assert(nu_map_get(map, "port") == NULL);

    assert(nu_map_delete(map, "port") == false);

    assert(nu_map_set(map, "port", &val2) == true);
    assert(nu_map_size(map) == 9);
    assert(*(int *)nu_map_get(map, "port") == 100);

    nu_map_destroy(map);
    nu_mm_destroy(mm);

    printf("All hashmap assertions passed successfully!\n");
    return 0;
}
