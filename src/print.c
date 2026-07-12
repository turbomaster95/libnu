#include <nu.h>
#include <stdarg.h>

typedef struct {
    char *dest;
    size_t capacity;
    size_t written;
} nu_fmt_out_t;

static inline void nu_fmt_putc(nu_fmt_out_t *out, char c) {
    if (out->written + 1 < out->capacity) {
        out->dest[out->written] = c;
    }
    out->written++;
}

static void nu_fmt_unsigned(nu_fmt_out_t *out, unsigned long long value, int base, int uppercase, int width, int zero_pad, int left_align) {
    char buffer[66]; // Large enough for base 2 tracking of uint64_t
    int pos = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (value == 0) {
        buffer[pos++] = '0';
    } else {
        while (value > 0) {
            buffer[pos++] = digits[value % base];
            value /= base;
        }
    }

    int padding = width - pos;
    if (!left_align && !zero_pad) {
        while (padding-- > 0) nu_fmt_putc(out, ' ');
    }
    if (!left_align && zero_pad) {
        while (padding-- > 0) nu_fmt_putc(out, '0');
    }
    for (int i = pos - 1; i >= 0; i--) {
        nu_fmt_putc(out, buffer[i]);
    }
    if (left_align) {
        while (padding-- > 0) nu_fmt_putc(out, ' ');
    }
}

int nu_vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    nu_fmt_out_t out = { .dest = str, .capacity = size, .written = 0 };
    if (!format) return 0;

    const char *p = format;
    while (*p) {
        if (*p != '%') {
            nu_fmt_putc(&out, *p++);
            continue;
        }

        p++; // Step over '%'

        // Reset format flag attributes per token
        int left_align = 0;
        int zero_pad = 0;
        int width = 0;
        int is_long_long = 0;
        int is_long = 0;

        while (*p == '-' || *p == '0') {
            if (*p == '-') left_align = 1;
            if (*p == '0') zero_pad = 1;
            p++;
        }
        if (left_align) zero_pad = 0; // Left alignment supersedes zero padding

        while (*p >= '0' && *p <= '9') {
            width = (width * 10) + (*p - '0');
            p++;
        }

        if (*p == 'l') {
            is_long = 1;
            p++;
            if (*p == 'l') {
                is_long_long = 1;
                p++;
            }
        }

        switch (*p) {
            case 'c': {
                char c = (char)va_arg(ap, int);
                nu_fmt_putc(&out, c);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char*);
                if (!s) s = "(null)";

                size_t len = 0;
                while (s[len]) len++;

                int padding = width - (int)len;
                if (!left_align) {
                    while (padding-- > 0) nu_fmt_putc(&out, ' ');
                }
                while (*s) {
                    nu_fmt_putc(&out, *s++);
                }
                if (left_align) {
                    while (padding-- > 0) nu_fmt_putc(&out, ' ');
                }
                break;
            }
            case 'd':
            case 'i': {
                long long val;
                if (is_long_long) val = va_arg(ap, long long);
                else if (is_long)  val = va_arg(ap, long);
                else               val = va_arg(ap, int);

                if (val < 0) {
                    nu_fmt_putc(&out, '-');
                    val = -val;
                    if (width > 0) width--;
                }
                nu_fmt_unsigned(&out, (unsigned long long)val, 10, 0, width, zero_pad, left_align);
                break;
            }
            case 'u': {
                unsigned long long val;
                if (is_long_long) val = va_arg(ap, unsigned long long);
                else if (is_long)  val = va_arg(ap, unsigned long);
                else               val = va_arg(ap, unsigned int);

                nu_fmt_unsigned(&out, val, 10, 0, width, zero_pad, left_align);
                break;
            }
            case 'x':
            case 'X': {
                unsigned long long val;
                if (is_long_long) val = va_arg(ap, unsigned long long);
                else if (is_long)  val = va_arg(ap, unsigned long);
                else               val = va_arg(ap, unsigned int);

                nu_fmt_unsigned(&out, val, 16, (*p == 'X'), width, zero_pad, left_align);
                break;
            }
            case 'p': {
                void *ptr = va_arg(ap, void*);
                nu_fmt_putc(&out, '0');
                nu_fmt_putc(&out, 'x');
                nu_fmt_unsigned(&out, (uintptr_t)ptr, 16, 0, width > 2 ? width - 2 : 0, zero_pad, left_align);
                break;
            }
            case '%': {
                nu_fmt_putc(&out, '%');
                break;
            }
            default: {
                nu_fmt_putc(&out, '%');
                nu_fmt_putc(&out, *p);
                break;
            }
        }
        p++;
    }

    if (out.capacity > 0) {
        if (out.written < out.capacity) {
            out.dest[out.written] = '\0';
        } else {
            out.dest[out.capacity - 1] = '\0';
        }
    }

    return (int)out.written;
}

int nu_snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = nu_vsnprintf(str, size, format, ap);
    va_end(ap);
    return result;
}

