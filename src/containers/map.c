#include <nu.h>
#include <stdint.h>

typedef struct {
    const char *key;
    void *value;
    bool is_occupied;
    bool is_deleted;
} nu_map_entry_t;

struct nu_map {
    nu_mm_t *mm;
    nu_map_entry_t *entries;
    size_t capacity;
    size_t size;
};

static uint32_t nu_map_hash(const char *key) {
    uint32_t hash = 2166136261u;
    while (*key) {
        hash ^= (unsigned char)*key++;
        hash *= 16777619u;
    }
    return hash;
}

static bool nu_map_streq(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

nu_map_t* nu_map_create(nu_mm_t *mm, size_t initial_capacity) {
    if (!mm) return NULL;
    if (initial_capacity < 8) initial_capacity = 8;

    nu_map_t *map = (nu_map_t *)nu_alloc(mm, sizeof(nu_map_t));
    if (!map) return NULL;

    map->mm = mm;
    map->capacity = initial_capacity;
    map->size = 0;
    map->entries = (nu_map_entry_t *)nu_alloc(mm, sizeof(nu_map_entry_t) * map->capacity);

    if (!map->entries) {
        nu_free(mm, map);
        return NULL;
    }

    for (size_t i = 0; i < map->capacity; i++) {
        map->entries[i].key = NULL;
        map->entries[i].value = NULL;
        map->entries[i].is_occupied = false;
        map->entries[i].is_deleted = false;
    }

    return map;
}

void nu_map_destroy(nu_map_t *map) {
    if (!map) return;
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].is_occupied) {
            nu_free(map->mm, (void *)map->entries[i].key);
        }
    }
    nu_free(map->mm, map->entries);
    nu_free(map->mm, map);
}

static bool nu_map_resize(nu_map_t *map, size_t new_capacity) {
    nu_map_entry_t *old_entries = map->entries;
    size_t old_capacity = map->capacity;

    nu_map_entry_t *new_entries = (nu_map_entry_t *)nu_alloc(map->mm, sizeof(nu_map_entry_t) * new_capacity);
    if (!new_entries) return false;

    for (size_t i = 0; i < new_capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = NULL;
        new_entries[i].is_occupied = false;
        new_entries[i].is_deleted = false;
    }

    map->entries = new_entries;
    map->capacity = new_capacity;
    map->size = 0;

    for (size_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].is_occupied) {
            nu_map_set(map, old_entries[i].key, old_entries[i].value);
            nu_free(map->mm, (void *)old_entries[i].key);
        }
    }

    nu_free(map->mm, old_entries);
    return true;
}

bool nu_map_set(nu_map_t *map, const char *key, void *value) {
    if (!map || !key) return false;

    if (map->size * 10 >= map->capacity * 7) {
        if (!nu_map_resize(map, map->capacity * 2)) return false;
    }

    uint32_t hash = nu_map_hash(key);
    size_t idx = hash % map->capacity;
    size_t tombstone_idx = (size_t)-1;

    while (map->entries[idx].is_occupied || map->entries[idx].is_deleted) {
        if (map->entries[idx].is_occupied) {
            if (nu_map_streq(map->entries[idx].key, key)) {
                map->entries[idx].value = value;
                return true;
            }
        } else if (map->entries[idx].is_deleted && tombstone_idx == (size_t)-1) {
            tombstone_idx = idx;
        }
        idx = (idx + 1) % map->capacity;
    }

    size_t target_idx = (tombstone_idx != (size_t)-1) ? tombstone_idx : idx;

    size_t klen = 0;
    while (key[klen]) klen++;
    char *key_copy = (char *)nu_alloc(map->mm, klen + 1);
    if (!key_copy) return false;
    for (size_t i = 0; i <= klen; i++) key_copy[i] = key[i];

    map->entries[target_idx].key = key_copy;
    map->entries[target_idx].value = value;
    map->entries[target_idx].is_occupied = true;
    map->entries[target_idx].is_deleted = false;
    map->size++;

    return true;
}

void* nu_map_get(nu_map_t *map, const char *key) {
    if (!map || !key) return NULL;

    uint32_t hash = nu_map_hash(key);
    size_t idx = hash % map->capacity;
    size_t start_idx = idx;

    while (map->entries[idx].is_occupied || map->entries[idx].is_deleted) {
        if (map->entries[idx].is_occupied && nu_map_streq(map->entries[idx].key, key)) {
            return map->entries[idx].value;
        }
        idx = (idx + 1) % map->capacity;
        if (idx == start_idx) break;
    }

    return NULL;
}

bool nu_map_delete(nu_map_t *map, const char *key) {
    if (!map || !key) return false;

    uint32_t hash = nu_map_hash(key);
    size_t idx = hash % map->capacity;
    size_t start_idx = idx;

    while (map->entries[idx].is_occupied || map->entries[idx].is_deleted) {
        if (map->entries[idx].is_occupied && nu_map_streq(map->entries[idx].key, key)) {
            nu_free(map->mm, (void *)map->entries[idx].key);
            map->entries[idx].key = NULL;
            map->entries[idx].value = NULL;
            map->entries[idx].is_occupied = false;
            map->entries[idx].is_deleted = true;
            map->size--;
            return true;
        }
        idx = (idx + 1) % map->capacity;
        if (idx == start_idx) break;
    }

    return false;
}

size_t nu_map_size(nu_map_t *map) {
    return map ? map->size : 0;
}

bool nu_map_iter_init(nu_map_t *map, nu_map_iter_t *iter) {
    if (!map || !iter) return false;
    iter->map = map;
    iter->bucket = 0;
    iter->entry = NULL;
    while (iter->bucket < map->capacity) {
        if (map->entries[iter->bucket].is_occupied) {
            iter->entry = &map->entries[iter->bucket];
            return true;
        }
        iter->bucket++;
    }
    return false;
}

bool nu_map_iter_next(nu_map_iter_t *iter, const char **out_key, void **out_value) {
    if (!iter || !iter->map) return false;
    if (iter->entry) {
        nu_map_entry_t *e = (nu_map_entry_t *)iter->entry;
        if (out_key) *out_key = e->key;
        if (out_value) *out_value = e->value;
        iter->entry = NULL;
    } else {
        if (out_key) *out_key = NULL;
        if (out_value) *out_value = NULL;
    }
    iter->bucket++;
    while (iter->bucket < iter->map->capacity) {
        if (iter->map->entries[iter->bucket].is_occupied) {
            iter->entry = &iter->map->entries[iter->bucket];
            return true;
        }
        iter->bucket++;
    }
    return iter->entry != NULL;
}
