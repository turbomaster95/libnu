#include <nu.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <sys/types.h>

extern long write(int fd, const void *buf, size_t count);

typedef struct {
    char *dest;
    size_t capacity;
    size_t written;
} nu_fmt_out_t;

static void nu_fmt_putc(nu_fmt_out_t *out, char c) {
    if (out->dest && out->capacity > 0 && out->written + 1 < out->capacity) {
        out->dest[out->written] = c;
    }

    if (out->written != (size_t)-1) {
        out->written++;
    }
}

static void nu_fmt_puts(nu_fmt_out_t *out, const char *str, size_t len) {
    if (!str) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        nu_fmt_putc(out, str[i]);
    }
}

static size_t nu_fmt_unsigned_digits(unsigned long long value, unsigned base) {
    size_t count = 1;

    while (value >= base) {
        value /= base;
        count++;
    }

    return count;
}

static void nu_fmt_unsigned(nu_fmt_out_t *out,
                            unsigned long long value,
                            unsigned base,
                            int uppercase,
                            int width,
                            int precision,
                            int zero_pad,
                            int left_align) {
    char buffer[65];
    size_t pos = 0;

    const char *digits = uppercase
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    if (value == 0) {
        if (precision != 0) {
            buffer[pos++] = '0';
        }
    } else {
        while (value != 0) {
            buffer[pos++] = digits[value % base];
            value /= base;
        }
    }

    size_t digit_count = pos;

    while ((int)digit_count < precision) {
        buffer[pos++] = '0';
        digit_count++;
    }

    int padding = width - (int)pos;

    if (!left_align && !zero_pad) {
        while (padding-- > 0) {
            nu_fmt_putc(out, ' ');
        }
    }

    if (!left_align && zero_pad && precision < 0) {
        while (padding-- > 0) {
            nu_fmt_putc(out, '0');
        }
    }

    for (size_t i = pos; i > 0; i--) {
        nu_fmt_putc(out, buffer[i - 1]);
    }

    if (left_align) {
        while (padding-- > 0) {
            nu_fmt_putc(out, ' ');
        }
    }
}

static void nu_fmt_signed(nu_fmt_out_t *out,
                          long long value,
                          int width,
                          int precision,
                          int zero_pad,
                          int left_align) {
    unsigned long long magnitude;

    if (value < 0) {
        magnitude = 0ULL - (unsigned long long)value;
    } else {
        magnitude = (unsigned long long)value;
    }

    size_t digits = nu_fmt_unsigned_digits(magnitude, 10);
    size_t precision_digits = digits;

    if (precision > (int)precision_digits) {
        precision_digits = (size_t)precision;
    }

    int total = (int)precision_digits + (value < 0 ? 1 : 0);
    int padding = width - total;

    if (!left_align && !zero_pad) {
        while (padding-- > 0) {
            nu_fmt_putc(out, ' ');
        }
    }

    if (value < 0) {
        nu_fmt_putc(out, '-');
    }

    if (!left_align && zero_pad && precision < 0) {
        while (padding-- > 0) {
            nu_fmt_putc(out, '0');
        }
    }

    nu_fmt_unsigned(out, magnitude, 10, 0, 0, precision, 0, 0);

    if (left_align) {
        while (padding-- > 0) {
            nu_fmt_putc(out, ' ');
        }
    }
}

static int nu_fmt_get_int_arg(va_list *ap) {
    return va_arg(*ap, int);
}

int nu_vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    nu_fmt_out_t out = {
        .dest = str,
        .capacity = size,
        .written = 0
    };

    if (!format) {
        if (str && size > 0) {
            str[0] = '\0';
        }
        return 0;
    }

    const char *p = format;

    while (*p) {
        if (*p != '%') {
            nu_fmt_putc(&out, *p++);
            continue;
        }

        p++;

        if (*p == '%') {
            nu_fmt_putc(&out, '%');
            p++;
            continue;
        }

        int left_align = 0;
        int zero_pad = 0;
        int plus = 0;
        int space = 0;
        int alternate = 0;
        int width = 0;
        int precision = -1;

        while (1) {
            if (*p == '-') {
                left_align = 1;
            } else if (*p == '0') {
                zero_pad = 1;
            } else if (*p == '+') {
                plus = 1;
            } else if (*p == ' ') {
                space = 1;
            } else if (*p == '#') {
                alternate = 1;
            } else {
                break;
            }

            p++;
        }

        if (*p == '*') {
            width = nu_fmt_get_int_arg(&ap);

            if (width < 0) {
                left_align = 1;
                width = -width;
            }

            p++;
        } else {
            while (*p >= '0' && *p <= '9') {
                if (width <= (INT_MAX - (*p - '0')) / 10) {
                    width = width * 10 + (*p - '0');
                } else {
                    width = INT_MAX;
                }
                p++;
            }
        }

        if (*p == '.') {
            p++;
            precision = 0;

            if (*p == '*') {
                precision = nu_fmt_get_int_arg(&ap);

                if (precision < 0) {
                    precision = -1;
                }

                p++;
            } else {
                while (*p >= '0' && *p <= '9') {
                    if (precision <= (INT_MAX - (*p - '0')) / 10) {
                        precision = precision * 10 + (*p - '0');
                    } else {
                        precision = INT_MAX;
                    }
                    p++;
                }
            }
        }

        int length = 0;

        if (*p == 'z') {
            length = 3;
            p++;
        } else if (*p == 'l') {
            length = 1;
            p++;

            if (*p == 'l') {
                length = 2;
                p++;
            }
        } else if (*p == 'h') {
            length = -1;
            p++;

            if (*p == 'h') {
                length = -2;
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
                const char *s = va_arg(ap, const char *);

                if (!s) {
                    s = "(null)";
                }

                size_t len = 0;

                while (s[len] && (precision < 0 || (int)len < precision)) {
                    len++;
                }

                int padding = width - (int)len;

                if (!left_align) {
                    while (padding-- > 0) {
                        nu_fmt_putc(&out, ' ');
                    }
                }

                nu_fmt_puts(&out, s, len);

                if (left_align) {
                    while (padding-- > 0) {
                        nu_fmt_putc(&out, ' ');
                    }
                }

                break;
            }

            case 'd':
            case 'i': {
                long long value;

                if (length == 2) {
                    value = va_arg(ap, long long);
                } else if (length == 1) {
                    value = va_arg(ap, long);
                } else if (length == 3) {
                    value = (long long)va_arg(ap, ssize_t);
                } else {
                    value = va_arg(ap, int);
                }

                nu_fmt_signed(&out,
                              value,
                              width,
                              precision,
                              zero_pad,
                              left_align);

                break;
            }

            case 'u':
            case 'x':
            case 'X':
            case 'o':
            case 'b': {
                unsigned long long value;

                if (length == 2) {
                    value = va_arg(ap, unsigned long long);
                } else if (length == 1) {
                    value = va_arg(ap, unsigned long);
                } else if (length == 3) {
                    value = (unsigned long long)va_arg(ap, size_t);
                } else {
                    value = va_arg(ap, unsigned int);
                }

                unsigned base = 10;

                if (*p == 'x' || *p == 'X') {
                    base = 16;
                } else if (*p == 'o') {
                    base = 8;
                } else if (*p == 'b') {
                    base = 2;
                }

                int prefix = 0;

                if (alternate) {
                    if (base == 16 && value != 0) {
                        nu_fmt_putc(&out, '0');
                        nu_fmt_putc(&out, *p == 'X' ? 'X' : 'x');
                        prefix = 2;
                    } else if (base == 8 && value != 0) {
                        nu_fmt_putc(&out, '0');
                        prefix = 1;
                    } else if (base == 2 && value != 0) {
                        nu_fmt_putc(&out, '0');
                        nu_fmt_putc(&out, 'b');
                        prefix = 2;
                    }
                }

                if (prefix) {
                    if (width >= prefix) {
                        width -= prefix;
                    } else {
                        width = 0;
                    }
                }

                nu_fmt_unsigned(&out,
                                value,
                                base,
                                *p == 'X',
                                width,
                                precision,
                                zero_pad,
                                left_align);

                break;
            }

            case 'p': {
                uintptr_t value = (uintptr_t)va_arg(ap, void *);

                nu_fmt_putc(&out, '0');
                nu_fmt_putc(&out, 'x');

                int pointer_width = (int)(sizeof(uintptr_t) * 2);

                nu_fmt_unsigned(&out,
                                (unsigned long long)value,
                                16,
                                0,
                                pointer_width,
                                pointer_width,
                                1,
                                0);

                break;
            }

            default:
                nu_fmt_putc(&out, '%');

                if (*p) {
                    nu_fmt_putc(&out, *p);
                }

                break;
        }

        if (*p) {
            p++;
        }
    }

    if (str && size > 0) {
        size_t terminator = out.written < size
            ? out.written
            : size - 1;

        str[terminator] = '\0';
    }

    if (out.written > (size_t)INT_MAX) {
        return -1;
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

int nu_fd_write(int fd, const char *buf, size_t len) {
    if (!buf || len == 0) {
        return 0;
    }

    size_t total = 0;

    while (total < len) {
        long written = write(fd, buf + total, len - total);

        if (written < 0) {
            return -1;
        }

        if (written == 0) {
            return -1;
        }

        total += (size_t)written;
    }

    if (total > (size_t)INT_MAX) {
        return -1;
    }

    return (int)total;
}

int nu_printf(const char *format, ...) {
    if (!format) {
        return 0;
    }

    char buffer[1024];

    va_list ap;
    va_start(ap, format);

    va_list copy;
    va_copy(copy, ap);

    int needed = nu_vsnprintf(NULL, 0, format, copy);

    va_end(copy);

    if (needed < 0) {
        va_end(ap);
        return -1;
    }

    if ((size_t)needed < sizeof(buffer)) {
        int result = nu_vsnprintf(buffer, sizeof(buffer), format, ap);
        va_end(ap);

        if (result < 0) {
            return -1;
        }

        return nu_fd_write(1, buffer, (size_t)result);
    }

    va_end(ap);

    size_t length = (size_t)needed;

    char *large = (char *)__builtin_alloca(length + 1);
    if (!large) {
        return -1;
    }

    va_start(ap, format);
    int result = nu_vsnprintf(large, length + 1, format, ap);
    va_end(ap);

    if (result < 0) {
        return -1;
    }

    return nu_fd_write(1, large, (size_t)result);
}
