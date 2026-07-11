#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <nu.h>

static size_t nu_json_measure(const nu_ast_node_t *node) {
    if (!node) return 0;
    size_t size = 0;

    switch (node->type) {
        case NU_AST_JSON_NULL:   return 4; /* "null" */
        case NU_AST_JSON_BOOL:   return node->val.i64 ? 4 : 5; /* "true" / "false" */
        case NU_AST_JSON_INT:    return snprintf(NULL, 0, "%lld", (long long)node->val.i64);
        case NU_AST_JSON_FLOAT:  return snprintf(NULL, 0, "%g", node->val.f64);
        case NU_AST_JSON_STRING: return strlen(node->val.str) + 2; /* includes "" */
        
        case NU_AST_JSON_ARRAY: {
            size += 2; /* [] */
            nu_ast_node_t *curr = node->first_child;
            while (curr) {
                size += nu_json_measure(curr);
                if (curr->next_sibling) size += 1; /* comma */
                curr = curr->next_sibling;
            }
            return size;
        }
        case NU_AST_JSON_OBJECT: {
            size += 2; /* {} */
            nu_ast_node_t *curr = node->first_child;
            while (curr) {
                size += nu_json_measure(curr);
                if (curr->next_sibling) size += 1; /* comma */
                curr = curr->next_sibling;
            }
            return size;
        }
        case NU_AST_JSON_PAIR: {
            /* "key": value */
            size += strlen(node->val.str) + 3; /* "key": */
            size += nu_json_measure(node->first_child);
            return size;
        }
    }
    return 0;
}

static void nu_json_serialize(char *dest, size_t *offset, const nu_ast_node_t *node) {
    if (!node) return;

    switch (node->type) {
        case NU_AST_JSON_NULL:
            *offset += sprintf(dest + *offset, "null");
            break;
        case NU_AST_JSON_BOOL:
            *offset += sprintf(dest + *offset, node->val.i64 ? "true" : "false");
            break;
        case NU_AST_JSON_INT:
            *offset += sprintf(dest + *offset, "%lld", (long long)node->val.i64);
            break;
        case NU_AST_JSON_FLOAT:
            *offset += sprintf(dest + *offset, "%g", node->val.f64);
            break;
        case NU_AST_JSON_STRING:
            *offset += sprintf(dest + *offset, "\"%s\"", node->val.str);
            break;
        case NU_AST_JSON_ARRAY: {
            dest[(*offset)++] = '[';
            nu_ast_node_t *curr = node->first_child;
            while (curr) {
                nu_json_serialize(dest, offset, curr);
                if (curr->next_sibling) dest[(*offset)++] = ',';
                curr = curr->next_sibling;
            }
            dest[(*offset)++] = ']';
            break;
        }
        case NU_AST_JSON_OBJECT: {
            dest[(*offset)++] = '{';
            nu_ast_node_t *curr = node->first_child;
            while (curr) {
                nu_json_serialize(dest, offset, curr);
                if (curr->next_sibling) dest[(*offset)++] = ',';
                curr = curr->next_sibling;
            }
            dest[(*offset)++] = '}';
            break;
        }
        case NU_AST_JSON_PAIR: {
            *offset += sprintf(dest + *offset, "\"%s\":", node->val.str);
            nu_json_serialize(dest, offset, node->first_child);
            break;
        }
    }
}

char* nu_json_encode(nu_mm_t *mm, const nu_ast_node_t *root) {
    if (!mm || !root) return NULL;
    size_t total_len = nu_json_measure(root);
    char *buf = (char *)nu_alloc(mm, total_len + 1);
    if (!buf) return NULL;
    
    size_t offset = 0;
    nu_json_serialize(buf, &offset, root);
    buf[offset] = '\0';
    return buf;
}

typedef enum {
    JSON_TOK_EOF, JSON_TOK_ERROR,
    JSON_TOK_LBRACE, JSON_TOK_RBRACE, JSON_TOK_LBRACK, JSON_TOK_RBRACK,
    JSON_TOK_COLON, JSON_TOK_COMMA,
    JSON_TOK_STRING, JSON_TOK_NUMBER, JSON_TOK_TRUE, JSON_TOK_FALSE, JSON_TOK_NULL
} json_tok_kind_t;

typedef struct {
    const char *src;
    size_t pos;
    char current;
} json_lex_t;

static void json_adv(json_lex_t *l) {
    l->current = l->src[l->pos] ? l->src[++l->pos] : '\0';
}

static void json_skip_ws(json_lex_t *l) {
    while (l->src[l->pos] == ' ' || l->src[l->pos] == '\t' || 
           l->src[l->pos] == '\n' || l->src[l->pos] == '\r') {
        l->pos++;
    }
    l->current = l->src[l->pos];
}

static json_tok_kind_t json_next_token(json_lex_t *l, const char **start, size_t *len) {
    json_skip_ws(l);
    *start = &l->src[l->pos];
    *len = 0;

    if (l->current == '\0') return JSON_TOK_EOF;

    char c = l->current;
    json_adv(l);

    switch (c) {
        case '{': return JSON_TOK_LBRACE;
        case '}': return JSON_TOK_RBRACE;
        case '[': return JSON_TOK_LBRACK;
        case ']': return JSON_TOK_RBRACK;
        case ':': return JSON_TOK_COLON;
        case ',': return JSON_TOK_COMMA;
        case '"':
            *start = &l->src[l->pos]; /* actual text inside quotes */
            while (l->current != '"' && l->current != '\0') {
                (*len)++;
                json_adv(l);
            }
            if (l->current == '"') json_adv(l); /* pass closing quote */
            return JSON_TOK_STRING;
    }

    if ((c >= '0' && c <= '9') || c == '-') {
        (*len) = 1;
        while ((l->current >= '0' && l->current <= '9') || l->current == '.') {
            (*len)++;
            json_adv(l);
        }
        return JSON_TOK_NUMBER;
    }

    /* Keywords match helper */
    size_t k_pos = l->pos - 1;
    if (strncmp(&l->src[k_pos], "true", 4) == 0)  { l->pos += 3; json_adv(l); return JSON_TOK_TRUE; }
    if (strncmp(&l->src[k_pos], "false", 5) == 0) { l->pos += 4; json_adv(l); return JSON_TOK_FALSE; }
    if (strncmp(&l->src[k_pos], "null", 4) == 0)  { l->pos += 3; json_adv(l); return JSON_TOK_NULL; }

    return JSON_TOK_ERROR;
}

static nu_ast_node_t* json_parse_value(nu_mm_t *mm, json_lex_t *l) {
    const char *tok_data;
    size_t tok_len;
    json_lex_t lookahead = *l;
    json_tok_kind_t kind = json_next_token(&lookahead, &tok_data, &tok_len);

    nu_ast_node_t *node = (nu_ast_node_t *)nu_alloc(mm, sizeof(nu_ast_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(nu_ast_node_t));

    *l = lookahead; /* Advance master lexer state */

    switch (kind) {
        case JSON_TOK_NULL:
            node->type = NU_AST_JSON_NULL;
            return node;
        case JSON_TOK_TRUE:
            node->type = NU_AST_JSON_BOOL;
            node->val.i64 = 1;
            return node;
        case JSON_TOK_FALSE:
            node->type = NU_AST_JSON_BOOL;
            node->val.i64 = 0;
            return node;
        case JSON_TOK_NUMBER: {
            char scratch[64];
            size_t copy_len = tok_len < 63 ? tok_len : 63;
            strncpy(scratch, tok_data, copy_len);
            scratch[copy_len] = '\0';
            if (strchr(scratch, '.')) {
                node->type = NU_AST_JSON_FLOAT;
                node->val.f64 = strtod(scratch, NULL);
            } else {
                node->type = NU_AST_JSON_INT;
                node->val.i64 = strtoll(scratch, NULL, 10);
            }
            return node;
        }
        case JSON_TOK_STRING: {
            node->type = NU_AST_JSON_STRING;
            char *s_copy = (char *)nu_alloc(mm, tok_len + 1);
            if (s_copy) {
                strncpy(s_copy, tok_data, tok_len);
                s_copy[tok_len] = '\0';
            }
            node->val.str = s_copy;
            return node;
        }
        case JSON_TOK_LBRACK: {
            node->type = NU_AST_JSON_ARRAY;
            nu_ast_node_t *last = NULL;
            json_skip_ws(l);
            if (l->current == ']') { json_adv(l); return node; }
            
            while (1) {
                nu_ast_node_t *item = json_parse_value(mm, l);
                if (!node->first_child) node->first_child = item;
                if (last) last->next_sibling = item;
                last = item;

                const char *s; size_t len;
                lookahead = *l;
                json_tok_kind_t next = json_next_token(&lookahead, &s, &len);
                if (next == JSON_TOK_COMMA) { *l = lookahead; }
                else if (next == JSON_TOK_RBRACK) { *l = lookahead; break; }
                else break;
            }
            return node;
        }
        case JSON_TOK_LBRACE: {
            node->type = NU_AST_JSON_OBJECT;
            nu_ast_node_t *last = NULL;
            json_skip_ws(l);
            if (l->current == '}') { json_adv(l); return node; }

            while (1) {
                const char *k_str; size_t k_len;
                json_tok_kind_t k_kind = json_next_token(l, &k_str, &k_len);
                if (k_kind != JSON_TOK_STRING) break;

                const char *s; size_t len;
                json_tok_kind_t colon = json_next_token(l, &s, &len);
                if (colon != JSON_TOK_COLON) break;

                nu_ast_node_t *pair = (nu_ast_node_t *)nu_alloc(mm, sizeof(nu_ast_node_t));
                if (!pair) break;
                memset(pair, 0, sizeof(nu_ast_node_t));
                pair->type = NU_AST_JSON_PAIR;

                char *k_copy = (char *)nu_alloc(mm, k_len + 1);
                if (k_copy) { strncpy(k_copy, k_str, k_len); k_copy[k_len] = '\0'; }
                pair->val.str = k_copy;
                pair->first_child = json_parse_value(mm, l);

                if (!node->first_child) node->first_child = pair;
                if (last) last->next_sibling = pair;
                last = pair;

                lookahead = *l;
                json_tok_kind_t next = json_next_token(&lookahead, &s, &len);
                if (next == JSON_TOK_COMMA) { *l = lookahead; }
                else if (next == JSON_TOK_RBRACE) { *l = lookahead; break; }
                else break;
            }
            return node;
        }
        default:
            break;
    }
    return node;
}

nu_ast_node_t* nu_json_decode(nu_mm_t *mm, const char *json) {
    if (!mm || !json) return NULL;
    json_lex_t l = { .src = json, .pos = 0, .current = json[0] };
    return json_parse_value(mm, &l);
}
