#include <nu.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

static bool nu_str_is_delim(char c, const char *delim) {
    if (!delim) return false;

    while (*delim) {
        if (c == *delim) return true;
        delim++;
    }

    return false;
}

char **nu_str_split(nu_mm_t *mm, const char *str, const char *delim, int *out_count) {
    if (out_count) {
        *out_count = 0;
    }

    if (!mm || !str || !delim || !out_count) {
        return NULL;
    }

    size_t count = 0;
    const char *p = str;

    while (*p) {
        while (*p && nu_str_is_delim(*p, delim)) {
            p++;
        }

        if (!*p) {
            break;
        }

        count++;

        while (*p && !nu_str_is_delim(*p, delim)) {
            p++;
        }
    }

    if (count == 0) {
        return NULL;
    }

    if (count > ((size_t)-1) / sizeof(char *)) {
        return NULL;
    }

    char **result = (char **)nu_alloc(mm, count * sizeof(char *));
    if (!result) {
        return NULL;
    }

    p = str;

    for (size_t i = 0; i < count; i++) {
        while (*p && nu_str_is_delim(*p, delim)) {
            p++;
        }

        const char *start = p;

        while (*p && !nu_str_is_delim(*p, delim)) {
            p++;
        }

        size_t len = (size_t)(p - start);

        if (len == (size_t)-1) {
            nu_free(mm, result);
            return NULL;
        }

        result[i] = (char *)nu_alloc(mm, len + 1);
        if (!result[i]) {
            for (size_t j = 0; j < i; j++) {
                nu_free(mm, result[j]);
            }
            nu_free(mm, result);
            return NULL;
        }

        memcpy(result[i], start, len);
        result[i][len] = '\0';
    }

    *out_count = (int)count;
    return result;
}

void nu_str_free_list(nu_mm_t *mm, char **list, int count) {
    if (!mm || !list || count < 0) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if (list[i]) {
            nu_free(mm, list[i]);
        }
    }

    nu_free(mm, list);
}

char *nu_str_trim(char *str) {
    if (!str) {
        return NULL;
    }

    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    char *end = str + strlen(str);

    while (end > str && isspace((unsigned char)end[-1])) {
        end--;
    }

    *end = '\0';

    return str;
}
