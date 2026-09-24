#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <nu.h>

nu_span_t nu_span_from_str(const char *str) {
    nu_span_t span = { NULL, 0 };

    if (str) {
        span.ptr = str;
        span.len = strlen(str);
    }

    return span;
}

nu_span_t nu_span_from_parts(const char *ptr, size_t len) {
    nu_span_t span = { ptr, len };

    if (!ptr) {
        span.len = 0;
    }

    return span;
}

bool nu_span_eq(nu_span_t span, const char *str) {
    if (!str) {
        return span.len == 0;
    }

    size_t len = strlen(str);

    if (span.len != len) {
        return false;
    }

    if (len == 0) {
        return true;
    }

    if (!span.ptr) {
        return false;
    }

    return memcmp(span.ptr, str, len) == 0;
}

bool nu_span_eq_span(nu_span_t span1, nu_span_t span2) {
    if (span1.len != span2.len) {
        return false;
    }

    if (span1.len == 0) {
        return true;
    }

    if (!span1.ptr || !span2.ptr) {
        return false;
    }

    return memcmp(span1.ptr, span2.ptr, span1.len) == 0;
}

nu_span_t nu_span_trim(nu_span_t span) {
    if (!span.ptr || span.len == 0) {
        return span;
    }

    while (span.len > 0) {
        unsigned char c = (unsigned char)span.ptr[0];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            span.ptr++;
            span.len--;
        } else {
            break;
        }
    }

    while (span.len > 0) {
        unsigned char c = (unsigned char)span.ptr[span.len - 1];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            span.len--;
        } else {
            break;
        }
    }

    return span;
}

nu_span_t nu_span_split_next(nu_span_t *span, char delim) {
    nu_span_t token = { NULL, 0 };

    if (!span || !span->ptr || span->len == 0) {
        return token;
    }

    for (size_t i = 0; i < span->len; i++) {
        if (span->ptr[i] == delim) {
            token.ptr = span->ptr;
            token.len = i;

            span->ptr += i + 1;
            span->len -= i + 1;

            return token;
        }
    }

    token = *span;
    span->ptr = NULL;
    span->len = 0;

    return token;
}
