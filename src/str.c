#include <nu.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char** nu_str_split(const char *str, const char *delim, int *out_count) {
    char *s = strdup(str);
    if (!s) return NULL;

    int count = 0;
    char *token = strtok(s, delim);
    while (token) {
        count++;
        token = strtok(NULL, delim);
    }
    free(s);

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    char **result = malloc(sizeof(char*) * count);
    if (!result) return NULL;
    
    s = strdup(str);
    token = strtok(s, delim);
    for (int i = 0; i < count; i++) {
        result[i] = strdup(token);
        token = strtok(NULL, delim);
    }
    free(s);

    *out_count = count;
    return result;
}

void nu_str_free_list(char **list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
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
