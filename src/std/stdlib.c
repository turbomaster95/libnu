#include <limits.h>
#include <nus.h>

int nu_atoi(const char *s) {
    int n = 0, neg = 0;
    while (nu_isspace(*s)) s++;
    switch (*s) {
        case '-': neg = 1; // fallthrough
        case '+': s++;
    }
    while (nu_isdigit(*s)) {
        // Safe overflow-resistant accumulation
        n = 10 * n - (*s++ - '0');
    }
    return neg ? n : -n;
}

long nu_strtol(const char *restrict s, char **restrict endptr, int base) {
    const char *p = s;
    unsigned long long val = 0;
    int neg = 0, c;

    while (nu_isspace(*p)) p++;

    if (*p == '-') {
        neg = 1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    if (base == 0) {
        if (*p == '0') {
            p++;
            if ((*p | 32) == 'x') {
                p++;
                base = 16;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*p == '0' && (p[1] | 32) == 'x') {
            p += 2;
        }
    }

    while (1) {
        c = *p;
        if (nu_isdigit(c)) {
            c -= '0';
        } else if (nu_isalpha(c)) {
            c = (c | 32) - 'a' + 10;
        } else {
            break;
        }
        if (c >= base) break;

        // Prevent overflow
        if (val > (ULLONG_MAX - c) / base) {
            val = ULLONG_MAX;
        } else {
            val = val * base + c;
        }
        p++;
    }

    if (endptr) {
        *endptr = (char *)(p == s ? s : p);
    }

    if (val == ULLONG_MAX) {
        return neg ? LONG_MIN : LONG_MAX;
    }

    long long result = neg ? -(long long)val : (long long)val;
    if (result < LONG_MIN) return LONG_MIN;
    if (result > LONG_MAX) return LONG_MAX;

    return (long)result;
}

int nu_abs(int j) {
    return j < 0 ? -j : j;
}

// LCG (Linear Congruential Generator) for lightweight random generation
static unsigned long nu_rand_next = 1;

int nu_rand(void) {
    nu_rand_next = nu_rand_next * 1103515245 + 12345;
    return (unsigned int)(nu_rand_next / 65536) % 32768;
}

void nu_srand(unsigned int seed) {
    nu_rand_next = seed;
}
