#include <nu.h>
#include <stdint.h>
#include <stdarg.h>

nu_ast_t* nu_ast_create(nu_mm_t *mm) {
    if (!mm) return NULL;
    nu_ast_t *ast = (nu_ast_t *)nu_alloc(mm, sizeof(nu_ast_t));
    if (!ast) return NULL;
    ast->mm = mm;
    ast->root = NULL;
    return ast;
}

nu_ast_node_t* nu_ast_new_node(nu_ast_t *ast, uint32_t type) {
    if (!ast) return NULL;
    nu_ast_node_t *node = (nu_ast_node_t *)nu_alloc(ast->mm, sizeof(nu_ast_node_t));
    if (!node) return NULL;

    node->type = type;
    node->val.raw = NULL;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    return node;
}

nu_ast_node_t* nu_ast_new_branch(nu_ast_t *ast, uint32_t type, size_t child_count, ...) {
    nu_ast_node_t *parent = nu_ast_new_node(ast, type);
    if (!parent) return NULL;

    va_list args;
    va_start(args, child_count);
    for (size_t i = 0; i < child_count; i++) {
        nu_ast_node_t *child = va_arg(args, nu_ast_node_t *);
        if (child) {
            nu_ast_add_child(parent, child);
        }
    }
    va_end(args);

    return parent;
}

void nu_ast_add_child(nu_ast_node_t *parent, nu_ast_node_t *child) {
    if (!parent || !child) return;

    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
    } else {
        parent->last_child->next_sibling = child;
        parent->last_child = child;
    }
}

void nu_ast_set_str(nu_ast_t *ast, nu_ast_node_t *node, const char *str, size_t len) {
    if (!ast || !node || !str || len == 0) return;

    char *copy = (char *)nu_alloc(ast->mm, len + 1);
    if (!copy) return;

    for (size_t i = 0; i < len; i++) {
        copy[i] = str[i];
    }
    copy[len] = '\0';
    node->val.str = copy;
}

void nu_ast_set_int(nu_ast_node_t *node, int64_t val) {
    if (node) node->val.i64 = val;
}

void nu_ast_set_float(nu_ast_node_t *node, double val) {
    if (node) node->val.f64 = val;
}

static void nu_ast_free_node_recursive(nu_mm_t *mm, nu_ast_node_t *node) {
    if (!node) return;

    nu_ast_node_t *current = node->first_child;
    while (current) {
        nu_ast_node_t *next = current->next_sibling;
        nu_ast_free_node_recursive(mm, current);
        current = next;
    }
    nu_free(mm, node);
}

void nu_ast_destroy(nu_ast_t *ast) {
    if (!ast) return;
    if (ast->root) {
        nu_ast_free_node_recursive(ast->mm, ast->root);
    }
    nu_free(ast->mm, ast);
}

