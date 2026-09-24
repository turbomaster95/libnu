#include <nu.h>
#include <string.h>

nu_trie_t* nu_trie_create(nu_mm_t *mm) {
    if (!mm) return NULL;
    nu_trie_t *trie = (nu_trie_t*)nu_alloc(mm, sizeof(nu_trie_t));
    if (!trie) return NULL;

    trie->mm = mm;
    trie->size = 0;
    trie->root = (nu_trie_node_t*)nu_alloc(mm, sizeof(nu_trie_node_t));
    if (!trie->root) {
        nu_free(mm, trie);
        return NULL;
    }
    memset(trie->root, 0, sizeof(nu_trie_node_t));
    return trie;
}

static nu_trie_node_t* find_child(nu_trie_node_t *parent, char c) {
    nu_trie_node_t *curr = parent->first_child;
    while (curr) {
        if (curr->edge_char == c) return curr;
        curr = curr->next_sibling;
    }
    return NULL;
}

bool nu_trie_insert(nu_trie_t *trie, const char *key, void *value) {
    if (!trie || !key) return false;

    nu_trie_node_t *curr = trie->root;
    for (size_t i = 0; key[i] != '\0'; i++) {
        char c = key[i];
        nu_trie_node_t *child = find_child(curr, c);
        
        if (!child) {
            child = (nu_trie_node_t*)nu_alloc(trie->mm, sizeof(nu_trie_node_t));
            if (!child) return false;
            memset(child, 0, sizeof(nu_trie_node_t));
            child->edge_char = c;
            
            /* Push to front of sibling list */
            child->next_sibling = curr->first_child;
            curr->first_child = child;
        }
        curr = child;
    }

    if (!curr->is_terminal) {
        curr->is_terminal = true;
        trie->size++;
    }
    curr->value = value;
    return true;
}

void* nu_trie_lookup(nu_trie_t *trie, const char *key) {
    if (!trie || !key) return NULL;

    nu_trie_node_t *curr = trie->root;
    for (size_t i = 0; key[i] != '\0'; i++) {
        curr = find_child(curr, key[i]);
        if (!curr) return NULL;
    }
    return curr->is_terminal ? curr->value : NULL;
}

static bool delete_helper(nu_trie_t *trie, nu_trie_node_t *curr, const char *key, size_t depth, bool *removed) {
    if (!curr) return false;

    if (key[depth] == '\0') {
        if (!curr->is_terminal) return false;
        curr->is_terminal = false;
        curr->value = NULL;
        trie->size--;
        *removed = true;
        return curr->first_child == NULL; /* Can be pruned if no children */
    }

    char c = key[depth];
    nu_trie_node_t *prev = NULL;
    nu_trie_node_t *child = curr->first_child;

    while (child && child->edge_char != c) {
        prev = child;
        child = child->next_sibling;
    }

    if (!child) return false;

    bool should_prune = delete_helper(trie, child, key, depth + 1, removed);

    if (should_prune) {
        if (prev) {
            prev->next_sibling = child->next_sibling;
        } else {
            curr->first_child = child->next_sibling;
        }
        nu_free(trie->mm, child);
        
        /* Parent can also be pruned if it has no text markings left and no children */
        return !curr->is_terminal && curr->first_child == NULL;
    }

    return false;
}

bool nu_trie_delete(nu_trie_t *trie, const char *key) {
    if (!trie || !key || *key == '\0') return false;
    bool removed = false;
    delete_helper(trie, trie->root, key, 0, &removed);
    return removed;
}

static void free_node(nu_mm_t *mm, nu_trie_node_t *node) {
    if (!node) return;
    nu_trie_node_t *curr = node->first_child;
    while (curr) {
        nu_trie_node_t *next = curr->next_sibling;
        free_node(mm, curr);
        curr = next;
    }
    nu_free(mm, node);
}

void nu_trie_destroy(nu_trie_t *trie) {
    if (!trie) return;
    free_node(trie->mm, trie->root);
    nu_free(trie->mm, trie);
}

