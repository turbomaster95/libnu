#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <nu.h>

static bool nu_json_measure_add(size_t *value, size_t amount) {
    if (*value > (size_t)-1 - amount) {
        return false;
    }

    *value += amount;
    return true;
}

static size_t nu_json_escape_len(const char *str) {
    if (!str) {
        return 0;
    }

    size_t len = 0;

    while (*str) {
        unsigned char c = (unsigned char)*str++;

        switch (c) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                if (len > (size_t)-1 - 2) {
                    return 0;
                }
                len += 2;
                break;

            default:
                if (c < 0x20) {
                    if (len > (size_t)-1 - 6) {
                        return 0;
                    }
                    len += 6;
                } else {
                    if (len == (size_t)-1) {
                        return 0;
                    }
                    len++;
                }
                break;
        }
    }

    return len;
}

static size_t nu_json_measure(const nu_ast_node_t *node) {
    if (!node) {
        return 0;
    }

    size_t size = 0;

    switch (node->type) {
        case NU_AST_JSON_NULL:
            return 4;

        case NU_AST_JSON_BOOL:
            return node->val.i64 ? 4 : 5;

        case NU_AST_JSON_INT: {
            char temp[64];

            int len = snprintf(temp,
                               sizeof(temp),
                               "%lld",
                               (long long)node->val.i64);

            return len < 0 ? 0 : (size_t)len;
        }

        case NU_AST_JSON_FLOAT: {
            char temp[128];

            int len = snprintf(temp,
                               sizeof(temp),
                               "%.17g",
                               node->val.f64);

            return len < 0 ? 0 : (size_t)len;
        }

        case NU_AST_JSON_STRING: {
            size_t escaped = nu_json_escape_len(node->val.str);

            if (escaped > (size_t)-1 - 2) {
                return 0;
            }

            return escaped + 2;
        }

        case NU_AST_JSON_ARRAY: {
            size = 2;

            for (const nu_ast_node_t *curr = node->first_child;
                 curr;
                 curr = curr->next_sibling) {
                size_t child = nu_json_measure(curr);

                if (!child || !nu_json_measure_add(&size, child)) {
                    return 0;
                }

                if (curr->next_sibling &&
                    !nu_json_measure_add(&size, 1)) {
                    return 0;
                }
            }

            return size;
        }

        case NU_AST_JSON_OBJECT: {
            size = 2;

            for (const nu_ast_node_t *curr = node->first_child;
                 curr;
                 curr = curr->next_sibling) {
                size_t child = nu_json_measure(curr);

                if (!child || !nu_json_measure_add(&size, child)) {
                    return 0;
                }

                if (curr->next_sibling &&
                    !nu_json_measure_add(&size, 1)) {
                    return 0;
                }
            }

            return size;
        }

        case NU_AST_JSON_PAIR: {
            size_t key_len = nu_json_escape_len(node->val.str);

            if (key_len > (size_t)-1 - 3) {
                return 0;
            }

            size = key_len + 3;

            if (!node->first_child) {
                return 0;
            }

            size_t value_len = nu_json_measure(node->first_child);

            if (!value_len || !nu_json_measure_add(&size, value_len)) {
                return 0;
            }

            return size;
        }

        default:
            return 0;
    }
}

static void nu_json_write_escape(char *dest, size_t *offset, const char *str) {
    if (!str) {
        return;
    }

    const char hex[] = "0123456789abcdef";

    while (*str) {
        unsigned char c = (unsigned char)*str++;

        switch (c) {
            case '"':
                dest[(*offset)++] = '\\';
                dest[(*offset)++] = '"';
                break;

            case '\\':
                dest[(*offset)++] = '\\';
                dest[(*offset)++] = '\\';
                break;

            case '\b':
                dest[(*offset)++] = '\\';
                dest[(*offset)++] = 'b';
                break;

            case '\f':
                dest[(*offset)++] = '\\';
                dest[(*offset)++] = 'f';
                break;

            case '\n':
                dest[(*offset)++] = '\\';
                dest[(*offset)++] = 'n';
                break;

            case '\r':
                dest[(*offset)++] = '\\';
                dest[(*offset)++] = 'r';
                break;

            case '\t':
                dest[(*offset)++] = '\\';
                dest[(*offset)++] = 't';
                break;

            default:
                if (c < 0x20) {
                    dest[(*offset)++] = '\\';
                    dest[(*offset)++] = 'u';
                    dest[(*offset)++] = '0';
                    dest[(*offset)++] = '0';
                    dest[(*offset)++] = hex[c >> 4];
                    dest[(*offset)++] = hex[c & 0x0f];
                } else {
                    dest[(*offset)++] = (char)c;
                }
                break;
        }
    }
}

static void nu_json_serialize(char *dest, size_t *offset, const nu_ast_node_t *node) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case NU_AST_JSON_NULL:
            memcpy(dest + *offset, "null", 4);
            *offset += 4;
            break;

        case NU_AST_JSON_BOOL:
            if (node->val.i64) {
                memcpy(dest + *offset, "true", 4);
                *offset += 4;
            } else {
                memcpy(dest + *offset, "false", 5);
                *offset += 5;
            }
            break;

        case NU_AST_JSON_INT: {
            int len = sprintf(dest + *offset,
                              "%lld",
                              (long long)node->val.i64);

            if (len > 0) {
                *offset += (size_t)len;
            }

            break;
        }

        case NU_AST_JSON_FLOAT: {
            int len = sprintf(dest + *offset,
                              "%.17g",
                              node->val.f64);

            if (len > 0) {
                *offset += (size_t)len;
            }

            break;
        }

        case NU_AST_JSON_STRING:
            dest[(*offset)++] = '"';
            nu_json_write_escape(dest, offset, node->val.str);
            dest[(*offset)++] = '"';
            break;

        case NU_AST_JSON_ARRAY: {
            dest[(*offset)++] = '[';

            const nu_ast_node_t *curr = node->first_child;

            while (curr) {
                nu_json_serialize(dest, offset, curr);

                if (curr->next_sibling) {
                    dest[(*offset)++] = ',';
                }

                curr = curr->next_sibling;
            }

            dest[(*offset)++] = ']';
            break;
        }

        case NU_AST_JSON_OBJECT: {
            dest[(*offset)++] = '{';

            const nu_ast_node_t *curr = node->first_child;

            while (curr) {
                nu_json_serialize(dest, offset, curr);

                if (curr->next_sibling) {
                    dest[(*offset)++] = ',';
                }

                curr = curr->next_sibling;
            }

            dest[(*offset)++] = '}';
            break;
        }

        case NU_AST_JSON_PAIR:
            dest[(*offset)++] = '"';
            nu_json_write_escape(dest, offset, node->val.str);
            dest[(*offset)++] = '"';
            dest[(*offset)++] = ':';

            nu_json_serialize(dest, offset, node->first_child);
            break;

        default:
            break;
    }
}

char *nu_json_encode(nu_mm_t *mm, const nu_ast_node_t *root) {
    if (!mm || !root) {
        return NULL;
    }

    size_t total_len = nu_json_measure(root);

    if (total_len == 0) {
        return NULL;
    }

    if (total_len == (size_t)-1) {
        return NULL;
    }

    char *buf = (char *)nu_alloc(mm, total_len + 1);

    if (!buf) {
        return NULL;
    }

    size_t offset = 0;

    nu_json_serialize(buf, &offset, root);

    buf[offset] = '\0';

    return buf;
}

typedef enum {
    JSON_TOK_EOF,
    JSON_TOK_ERROR,
    JSON_TOK_LBRACE,
    JSON_TOK_RBRACE,
    JSON_TOK_LBRACK,
    JSON_TOK_RBRACK,
    JSON_TOK_COLON,
    JSON_TOK_COMMA,
    JSON_TOK_STRING,
    JSON_TOK_NUMBER,
    JSON_TOK_TRUE,
    JSON_TOK_FALSE,
    JSON_TOK_NULL
} json_tok_kind_t;

typedef struct {
    const char *src;
    size_t pos;
    char current;
} json_lex_t;

typedef struct {
    const char *ptr;
    size_t len;
} json_token_t;

static void json_adv(json_lex_t *l) {
    if (!l || !l->src) {
        return;
    }

    if (l->src[l->pos] == '\0') {
        l->current = '\0';
        return;
    }

    l->pos++;
    l->current = l->src[l->pos];
}

static void json_skip_ws(json_lex_t *l) {
    if (!l || !l->src) {
        return;
    }

    while (l->current == ' ' ||
           l->current == '\t' ||
           l->current == '\n' ||
           l->current == '\r') {
        json_adv(l);
    }
}

static bool json_match_literal(json_lex_t *l, const char *literal) {
    size_t len = strlen(literal);

    if (strncmp(l->src + l->pos, literal, len) != 0) {
        return false;
    }

    char next = l->src[l->pos + len];

    if ((next >= 'a' && next <= 'z') ||
        (next >= 'A' && next <= 'Z') ||
        (next >= '0' && next <= '9') ||
        next == '_') {
        return false;
    }

    l->pos += len;
    l->current = l->src[l->pos];

    return true;
}

static bool json_parse_string_token(json_lex_t *l,
                                    json_token_t *token) {
    if (!l || !token || l->current != '"') {
        return false;
    }

    json_adv(l);

    const char *start = l->src + l->pos;

    size_t len = 0;

    while (l->current != '\0') {
        unsigned char c = (unsigned char)l->current;

        if (c == '"') {
            token->ptr = start;
            token->len = len;

            json_adv(l);

            return true;
        }

        if (c < 0x20) {
            return false;
        }

        if (c == '\\') {
            json_adv(l);

            if (l->current == '\0') {
                return false;
            }

            switch (l->current) {
                case '"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    json_adv(l);
                    break;

                case 'u':
                    json_adv(l);

                    for (int i = 0; i < 4; i++) {
                        char h = l->current;

                        bool valid =
                            (h >= '0' && h <= '9') ||
                            (h >= 'a' && h <= 'f') ||
                            (h >= 'A' && h <= 'F');

                        if (!valid) {
                            return false;
                        }

                        json_adv(l);
                    }

                    break;

                default:
                    return false;
            }

            len += 2;
            continue;
        }

        json_adv(l);
        len++;
    }

    return false;
}

static bool json_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool json_parse_number_token(json_lex_t *l, json_token_t *token) {
    const char *start = l->src + l->pos;

    if (l->current == '-') {
        json_adv(l);

        if (!json_is_digit(l->current)) {
            return false;
        }
    }

    if (l->current == '0') {
        json_adv(l);

        if (json_is_digit(l->current)) {
            return false;
        }
    } else {
        if (!json_is_digit(l->current)) {
            return false;
        }

        while (json_is_digit(l->current)) {
            json_adv(l);
        }
    }

    if (l->current == '.') {
        json_adv(l);

        if (!json_is_digit(l->current)) {
            return false;
        }

        while (json_is_digit(l->current)) {
            json_adv(l);
        }
    }

    if (l->current == 'e' || l->current == 'E') {
        json_adv(l);

        if (l->current == '+' || l->current == '-') {
            json_adv(l);
        }

        if (!json_is_digit(l->current)) {
            return false;
        }

        while (json_is_digit(l->current)) {
            json_adv(l);
        }
    }

    token->ptr = start;
    token->len = (size_t)((l->src + l->pos) - start);

    return true;
}

static json_tok_kind_t json_next_token(json_lex_t *l, json_token_t *token) {
    if (!l || !l->src || !token) {
        return JSON_TOK_ERROR;
    }

    json_skip_ws(l);

    token->ptr = l->src + l->pos;
    token->len = 0;

    switch (l->current) {
        case '\0':
            return JSON_TOK_EOF;

        case '{':
            json_adv(l);
            return JSON_TOK_LBRACE;

        case '}':
            json_adv(l);
            return JSON_TOK_RBRACE;

        case '[':
            json_adv(l);
            return JSON_TOK_LBRACK;

        case ']':
            json_adv(l);
            return JSON_TOK_RBRACK;

        case ':':
            json_adv(l);
            return JSON_TOK_COLON;

        case ',':
            json_adv(l);
            return JSON_TOK_COMMA;

        case '"':
            if (!json_parse_string_token(l, token)) {
                return JSON_TOK_ERROR;
            }
            return JSON_TOK_STRING;

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (!json_parse_number_token(l, token)) {
                return JSON_TOK_ERROR;
            }
            return JSON_TOK_NUMBER;

        case 't':
            if (json_match_literal(l, "true")) {
                return JSON_TOK_TRUE;
            }
            return JSON_TOK_ERROR;

        case 'f':
            if (json_match_literal(l, "false")) {
                return JSON_TOK_FALSE;
            }
            return JSON_TOK_ERROR;

        case 'n':
            if (json_match_literal(l, "null")) {
                return JSON_TOK_NULL;
            }
            return JSON_TOK_ERROR;

        default:
            return JSON_TOK_ERROR;
    }
}

static int json_hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

static bool json_append_utf8(char **dest, size_t *capacity, size_t *length, uint32_t codepoint) {
    size_t needed;

    if (codepoint <= 0x7f) {
        needed = 1;
    } else if (codepoint <= 0x7ff) {
        needed = 2;
    } else if (codepoint <= 0xffff) {
        needed = 3;
    } else if (codepoint <= 0x10ffff) {
        needed = 4;
    } else {
        return false;
    }

    if (*length > *capacity - needed - 1) {
        size_t new_capacity = *capacity ? *capacity : 32;

        while (new_capacity < *length + needed + 1) {
            if (new_capacity > (size_t)-1 / 2) {
                return false;
            }

            new_capacity *= 2;
        }

        char *new_dest = (char *)realloc(*dest, new_capacity);

        if (!new_dest) {
            return false;
        }

        *dest = new_dest;
        *capacity = new_capacity;
    }

    if (needed == 1) {
        (*dest)[(*length)++] = (char)codepoint;
    } else if (needed == 2) {
        (*dest)[(*length)++] = (char)(0xc0 | (codepoint >> 6));
        (*dest)[(*length)++] = (char)(0x80 | (codepoint & 0x3f));
    } else if (needed == 3) {
        (*dest)[(*length)++] = (char)(0xe0 | (codepoint >> 12));
        (*dest)[(*length)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        (*dest)[(*length)++] = (char)(0x80 | (codepoint & 0x3f));
    } else {
        (*dest)[(*length)++] = (char)(0xf0 | (codepoint >> 18));
        (*dest)[(*length)++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        (*dest)[(*length)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        (*dest)[(*length)++] = (char)(0x80 | (codepoint & 0x3f));
    }

    (*dest)[*length] = '\0';

    return true;
}

static char *json_decode_string(nu_mm_t *mm, const char *src, size_t len) {
    if (!mm || (!src && len != 0)) {
        return NULL;
    }

    char *out = (char *)nu_alloc(mm, len + 1);

    if (!out) {
        return NULL;
    }

    size_t out_len = 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)src[i];

        if (c != '\\') {
            out[out_len++] = (char)c;
            continue;
        }

        if (++i >= len) {
            nu_free(mm, out);
            return NULL;
        }

        switch (src[i]) {
            case '"':
                out[out_len++] = '"';
                break;

            case '\\':
                out[out_len++] = '\\';
                break;

            case '/':
                out[out_len++] = '/';
                break;

            case 'b':
                out[out_len++] = '\b';
                break;

            case 'f':
                out[out_len++] = '\f';
                break;

            case 'n':
                out[out_len++] = '\n';
                break;

            case 'r':
                out[out_len++] = '\r';
                break;

            case 't':
                out[out_len++] = '\t';
                break;

            case 'u': {
                if (i + 4 >= len) {
                    nu_free(mm, out);
                    return NULL;
                }

                uint32_t cp = 0;

                for (int j = 0; j < 4; j++) {
                    int v = json_hex_value(src[i + 1 + j]);

                    if (v < 0) {
                        nu_free(mm, out);
                        return NULL;
                    }

                    cp = (cp << 4) | (uint32_t)v;
                }

                i += 4;

                if (cp >= 0xd800 && cp <= 0xdbff) {
                    if (i + 6 >= len ||
                        src[i + 1] != '\\' ||
                        src[i + 2] != 'u') {
                        nu_free(mm, out);
                        return NULL;
                    }

                    uint32_t low = 0;

                    for (int j = 0; j < 4; j++) {
                        int v = json_hex_value(src[i + 3 + j]);

                        if (v < 0) {
                            nu_free(mm, out);
                            return NULL;
                        }

                        low = (low << 4) | (uint32_t)v;
                    }

                    if (low < 0xdc00 || low > 0xdfff) {
                        nu_free(mm, out);
                        return NULL;
                    }

                    cp = 0x10000 +
                         ((cp - 0xd800) << 10) +
                         (low - 0xdc00);

                    i += 6;
                } else if (cp >= 0xdc00 && cp <= 0xdfff) {
                    nu_free(mm, out);
                    return NULL;
                }

                if (cp <= 0x7f) {
                    out[out_len++] = (char)cp;
                } else if (cp <= 0x7ff) {
                    out[out_len++] = (char)(0xc0 | (cp >> 6));
                    out[out_len++] = (char)(0x80 | (cp & 0x3f));
                } else if (cp <= 0xffff) {
                    out[out_len++] = (char)(0xe0 | (cp >> 12));
                    out[out_len++] = (char)(0x80 | ((cp >> 6) & 0x3f));
                    out[out_len++] = (char)(0x80 | (cp & 0x3f));
                } else {
                    out[out_len++] = (char)(0xf0 | (cp >> 18));
                    out[out_len++] = (char)(0x80 | ((cp >> 12) & 0x3f));
                    out[out_len++] = (char)(0x80 | ((cp >> 6) & 0x3f));
                    out[out_len++] = (char)(0x80 | (cp & 0x3f));
                }

                break;
            }

            default:
                nu_free(mm, out);
                return NULL;
        }
    }

    out[out_len] = '\0';

    return out;
}

static nu_ast_node_t *json_parse_value(nu_mm_t *mm, json_lex_t *l, int depth);

static nu_ast_node_t *json_new_node(nu_mm_t *mm, uint32_t type) {
    nu_ast_node_t *node =
        (nu_ast_node_t *)nu_alloc(mm, sizeof(nu_ast_node_t));

    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(*node));
    node->type = type;

    return node;
}

static void json_free_node(nu_mm_t *mm, nu_ast_node_t *node) {
    if (!mm || !node) {
        return;
    }

    nu_ast_node_t *child = node->first_child;

    while (child) {
        nu_ast_node_t *next = child->next_sibling;
        json_free_node(mm, child);
        child = next;
    }

    if ((node->type == NU_AST_JSON_STRING ||
         node->type == NU_AST_JSON_PAIR) &&
        node->val.str) {
        nu_free(mm, node->val.str);
    }

    nu_free(mm, node);
}

static nu_ast_node_t *json_parse_array(nu_mm_t *mm, json_lex_t *l, int depth) {
    nu_ast_node_t *node = json_new_node(mm, NU_AST_JSON_ARRAY);

    if (!node) {
        return NULL;
    }

    json_skip_ws(l);

    if (l->current == ']') {
        json_adv(l);
        return node;
    }

    while (1) {
        nu_ast_node_t *item = json_parse_value(mm, l, depth + 1);

        if (!item) {
            json_free_node(mm, node);
            return NULL;
        }

        if (!node->first_child) {
            node->first_child = item;
            node->last_child = item;
        } else {
            node->last_child->next_sibling = item;
            node->last_child = item;
        }

        json_skip_ws(l);

        if (l->current == ']') {
            json_adv(l);
            return node;
        }

        if (l->current != ',') {
            json_free_node(mm, node);
            return NULL;
        }

        json_adv(l);

        json_skip_ws(l);

        if (l->current == ']') {
            json_free_node(mm, node);
            return NULL;
        }
    }
}

static nu_ast_node_t *json_parse_object(nu_mm_t *mm, json_lex_t *l, int depth) {
    nu_ast_node_t *node = json_new_node(mm, NU_AST_JSON_OBJECT);

    if (!node) {
        return NULL;
    }

    json_skip_ws(l);

    if (l->current == '}') {
        json_adv(l);
        return node;
    }

    while (1) {
        json_token_t token;
        json_tok_kind_t kind = json_next_token(l, &token);

        if (kind != JSON_TOK_STRING) {
            json_free_node(mm, node);
            return NULL;
        }

        char *key = json_decode_string(mm, token.ptr, token.len);

        if (!key) {
            json_free_node(mm, node);
            return NULL;
        }

        kind = json_next_token(l, &token);

        if (kind != JSON_TOK_COLON) {
            nu_free(mm, key);
            json_free_node(mm, node);
            return NULL;
        }

        nu_ast_node_t *value =
            json_parse_value(mm, l, depth + 1);

        if (!value) {
            nu_free(mm, key);
            json_free_node(mm, node);
            return NULL;
        }

        nu_ast_node_t *pair =
            json_new_node(mm, NU_AST_JSON_PAIR);

        if (!pair) {
            nu_free(mm, key);
            json_free_node(mm, value);
            json_free_node(mm, node);
            return NULL;
        }

        pair->val.str = key;
        pair->first_child = value;
        pair->last_child = value;

        if (!node->first_child) {
            node->first_child = pair;
            node->last_child = pair;
        } else {
            node->last_child->next_sibling = pair;
            node->last_child = pair;
        }

        json_skip_ws(l);

        if (l->current == '}') {
            json_adv(l);
            return node;
        }

        if (l->current != ',') {
            json_free_node(mm, node);
            return NULL;
        }

        json_adv(l);

        json_skip_ws(l);

        if (l->current == '}') {
            json_free_node(mm, node);
            return NULL;
        }
    }
}

static nu_ast_node_t *json_parse_value(nu_mm_t *mm, json_lex_t *l, int depth) {
    if (!mm || !l) {
        return NULL;
    }

    if (depth > 128) {
        return NULL;
    }

    json_token_t token;
    json_tok_kind_t kind = json_next_token(l, &token);

    switch (kind) {
        case JSON_TOK_NULL: {
            return json_new_node(mm, NU_AST_JSON_NULL);
        }

        case JSON_TOK_TRUE: {
            nu_ast_node_t *node =
                json_new_node(mm, NU_AST_JSON_BOOL);

            if (node) {
                node->val.i64 = 1;
            }

            return node;
        }

        case JSON_TOK_FALSE: {
            nu_ast_node_t *node =
                json_new_node(mm, NU_AST_JSON_BOOL);

            if (node) {
                node->val.i64 = 0;
            }

            return node;
        }

        case JSON_TOK_STRING: {
            nu_ast_node_t *node =
                json_new_node(mm, NU_AST_JSON_STRING);

            if (!node) {
                return NULL;
            }

            node->val.str =
                json_decode_string(mm, token.ptr, token.len);

            if (!node->val.str) {
                json_free_node(mm, node);
                return NULL;
            }

            return node;
        }

        case JSON_TOK_NUMBER: {
            char *scratch =
                (char *)nu_alloc(mm, token.len + 1);

            if (!scratch) {
                return NULL;
            }

            memcpy(scratch, token.ptr, token.len);
            scratch[token.len] = '\0';

            bool is_float =
                strchr(scratch, '.') != NULL ||
                strchr(scratch, 'e') != NULL ||
                strchr(scratch, 'E') != NULL;

            nu_ast_node_t *node =
                json_new_node(mm,
                              is_float
                                  ? NU_AST_JSON_FLOAT
                                  : NU_AST_JSON_INT);

            if (!node) {
                nu_free(mm, scratch);
                return NULL;
            }

            if (is_float) {
                char *end = NULL;
                double value = strtod(scratch, &end);

                if (!end || *end != '\0') {
                    nu_free(mm, scratch);
                    json_free_node(mm, node);
                    return NULL;
                }

                node->val.f64 = value;
            } else {
                char *end = NULL;
                long long value = strtoll(scratch, &end, 10);

                if (!end || *end != '\0') {
                    nu_free(mm, scratch);
                    json_free_node(mm, node);
                    return NULL;
                }

                node->val.i64 = (int64_t)value;
            }

            nu_free(mm, scratch);

            return node;
        }

        case JSON_TOK_LBRACK:
            return json_parse_array(mm, l, depth);

        case JSON_TOK_LBRACE:
            return json_parse_object(mm, l, depth);

        default:
            return NULL;
    }
}

nu_ast_node_t *nu_json_decode(nu_mm_t *mm, const char *json) {
    if (!mm || !json) {
        return NULL;
    }

    json_lex_t lexer = {
        .src = json,
        .pos = 0,
        .current = json[0]
    };

    nu_ast_node_t *root =
        json_parse_value(mm, &lexer, 0);

    if (!root) {
        return NULL;
    }

    json_skip_ws(&lexer);

    if (lexer.current != '\0') {
        json_free_node(mm, root);
        return NULL;
    }

    return root;
}

nu_ast_node_t *nu_json_get(const nu_ast_node_t *root, const char *keypath) {
    if (!root || !keypath || *keypath == '\0') {
        return (nu_ast_node_t *)root;
    }

    const nu_ast_node_t *curr = root;
    const char *p = keypath;

    while (*p && curr) {
        const char *next_dot = strchr(p, '.');
        size_t seg_len = next_dot
            ? (size_t)(next_dot - p)
            : strlen(p);

        if (seg_len == 0) {
            if (next_dot) {
                p = next_dot + 1;
                continue;
            }

            return NULL;
        }

        if (curr->type == NU_AST_JSON_OBJECT) {
            const nu_ast_node_t *pair = curr->first_child;
            curr = NULL;

            while (pair) {
                if (pair->type == NU_AST_JSON_PAIR &&
                    pair->val.str) {
                    size_t key_len = strlen(pair->val.str);

                    if (key_len == seg_len &&
                        memcmp(pair->val.str, p, seg_len) == 0) {
                        curr = pair->first_child;
                        break;
                    }
                }

                pair = pair->next_sibling;
            }
        } else if (curr->type == NU_AST_JSON_ARRAY) {
            size_t index = 0;

            if (seg_len == 0) {
                return NULL;
            }

            for (size_t i = 0; i < seg_len; i++) {
                if (p[i] < '0' || p[i] > '9') {
                    return NULL;
                }

                size_t digit = (size_t)(p[i] - '0');

                if (index > ((size_t)-1 - digit) / 10) {
                    return NULL;
                }

                index = index * 10 + digit;
            }

            const nu_ast_node_t *item = curr->first_child;

            while (item && index > 0) {
                item = item->next_sibling;
                index--;
            }

            curr = item;
        } else {
            return NULL;
        }

        if (!next_dot) {
            break;
        }

        p = next_dot + 1;
    }

    return (nu_ast_node_t *)curr;
}
