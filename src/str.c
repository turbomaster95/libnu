#include <nu.h>
#include <nus.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char** nu_str_split(nu_mm_t *mm, const char *str, const char *delim, int *out_count) {
    char *s = nu_strdup(str);
    if (!s) return NULL;

    int count = 0;
    char *token = nu_strtok(s, delim);
    while (token) {
        count++;
        token = nu_strtok(NULL, delim);
    }
    free(s);

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    char **result = nu_alloc(mm, sizeof(char*) * count);
    if (!result) return NULL;

    s = nu_strdup(str);
    token = nu_strtok(s, delim);
    for (int i = 0; i < count; i++) {
        result[i] = nu_strdup(token);
        token = nu_strtok(NULL, delim);
    }
    free(s);

    *out_count = count;
    return result;
}

void nu_str_free_list(nu_mm_t *mm, char **list, int count) {
    if (!list) return;
    if (!mm) return;

    for (int i = 0; i < count; i++) {
        nu_free(mm, list[i]);
    }
    nu_free(mm, list);
}

char* nu_str_trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}
