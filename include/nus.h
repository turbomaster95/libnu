#ifndef NUS_H // Nu Standard Library
#define NUS_H

#define NUS_HERE // Define this to let programs know that the Nu Standard Library is available.

#include <stddef.h>

#line 1 "nu_ctype.h"

static inline int nu_isdigit(int c) { 
	return (unsigned)c - '0' < 10; 
}

static inline int nu_isalpha(int c) { 
	return ((unsigned)c | 32) - 'a' < 26; 
}

static inline int nu_isalnum(int c) { 
	return nu_isdigit(c) || nu_isalpha(c); 
}

static inline int nu_isspace(int c) { 
	return c == ' ' || (unsigned)c - '\t' < 5; 
}

static inline int nu_isupper(int c) { 
	return (unsigned)c - 'A' < 26; 
}

static inline int nu_islower(int c) { 
	return (unsigned)c - 'a' < 26; 
}

static inline int nu_toupper(int c) { 
	return nu_islower(c) ? (c - 0x20) : c; 
}

static inline int nu_tolower(int c) { 
	return nu_isupper(c) ? (c + 0x20) : c; 
}

#line 1 "nu_string.h"

#define ARENA_SIZE 65536 // Sets the small arena for strdup

char *nu_strdup(const char *s);
size_t nu_strlen(const char *s);
char *nu_strlcpy(char *dest, const char *src, size_t n);
size_t nu_strnlen(const char *s, size_t maxlen);
int nu_strcmp(const char *s1, const char *s2);
int nu_strncmp(const char *s1, const char *s2, size_t n);
char *nu_strcpy(char *restrict dest, const char *restrict src);
char *nu_strncpy(char *restrict dest, const char *restrict src, size_t n);
char *nu_strcat(char *restrict dest, const char *restrict src);
char *nu_strncat(char *restrict dest, const char *restrict src, size_t n);
char *nu_strchr(const char *s, int c);
char *nu_strrchr(const char *s, int c);
char *nu_strstr(const char *haystack, const char *needle);

#line 1 "nu_stdlib.h"

int nu_atoi(const char *s);
long nu_strtol(const char *restrict s, char **restrict endptr, int base);
int nu_abs(int j);
int nu_rand(void);
void nu_srand(unsigned int seed);

#endif /* NUS_H */
