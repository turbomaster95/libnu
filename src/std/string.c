#include <stddef.h>
#include <stdint.h>
#include <nus.h>

static char g_arena[ARENA_SIZE];
static size_t g_arena_offset = 0;

size_t nu_strlen(const char *s) {
    const char *a = s;
    while (*s) s++;
    return s - a;
}

char *nu_strlcpy(char *dest, const char *src, size_t n) {
    if (n == 0) return dest;
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return dest;
}

size_t nu_strnlen(const char *s, size_t maxlen) {
    const char *a = s;
    while (maxlen-- && *s) s++;
    return s - a;
}

int nu_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int nu_strncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;
    while (n-- - 1 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *nu_strcpy(char *restrict dest, const char *restrict src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *nu_strncpy(char *restrict dest, const char *restrict src, size_t n) {
    char *d = dest;
    while (n > 0 && *src != '\0') {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dest;
}

char *nu_strcat(char *restrict dest, const char *restrict src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *nu_strncat(char *restrict dest, const char *restrict src, size_t n) {
    char *d = dest;
    while (*d) d++;
    while (n > 0 && *src != '\0') {
        *d++ = *src++;
        n--;
    }
    *d = '\0';
    return dest;
}

char *nu_strchr(const char *s, int c) {
    char ch = (char)c;
    while (*s != ch) {
        if (*s == '\0') return NULL;
        s++;
    }
    return (char *)s;
}

char *nu_strrchr(const char *s, int c) {
    char ch = (char)c;
    const char *last = NULL;
    do {
        if (*s == ch) last = s;
    } while (*s++);
    return (char *)last;
}

char *nu_strstr(const char *haystack, const char *needle) {
    if (*needle == '\0') return (char *)haystack;
    for (; *haystack != '\0'; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack;
            const char *n = needle;
            while (*h != '\0' && *n != '\0' && *h == *n) {
                h++;
                n++;
            }
            if (*n == '\0') return (char *)haystack;
        }
    }
    return NULL;
}

char *nu_strdup(const char *s) {
    size_t len = nu_strlen(s) + 1;

    if (g_arena_offset + len > ARENA_SIZE) {
        return NULL;
    }
    char *dest = &g_arena[g_arena_offset];
    nu_strlcpy(dest, s, len);
    g_arena_offset += len;
    return dest;
}
