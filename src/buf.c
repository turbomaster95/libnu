#include <string.h>
#include <nu.h>

#define NU_BUF_DEFAULT_MIN_CAP 16

void nu_buf_init(nu_buf_t *buf, nu_mm_t *mm, size_t initial_cap) {
    if (!buf) return;
    buf->mm = mm;
    buf->size = 0;
    buf->capacity = (initial_cap > 0) ? initial_cap : NU_BUF_DEFAULT_MIN_CAP;
    buf->data = (uint8_t *)nu_alloc(mm, buf->capacity);

    // Fallback if allocation fails immediately
    if (!buf->data) {
        buf->capacity = 0;
    }
}

void nu_buf_reserve(nu_buf_t *buf, size_t new_cap) {
    if (!buf || new_cap <= buf->capacity) return;

    if (!buf->data) {
        buf->data = (uint8_t *)nu_alloc(buf->mm, new_cap);
        if (buf->data) {
            buf->capacity = new_cap;
        }
        return;
    }

    uint8_t *new_ptr = (uint8_t *)nu_realloc(buf->mm, buf->data, new_cap);
    if (new_ptr) {
        buf->data = new_ptr;
        buf->capacity = new_cap;
    }
}

void nu_buf_append(nu_buf_t *buf, const void *data, size_t len) {
    if (!buf || !data || len == 0) return;

    size_t required = buf->size + len;
    if (required > buf->capacity) {
        size_t next_cap = buf->capacity * 2;
        if (next_cap < required) {
            next_cap = required;
        }
        nu_buf_reserve(buf, next_cap);
    }

    // Double-check allocator successfully served the reservation
    if (buf->data && buf->size + len <= buf->capacity) {
        memcpy(buf->data + buf->size, data, len);
        buf->size += len;
    }
}

void nu_buf_append_val(nu_buf_t *buf, uint8_t byte) {
    nu_buf_append(buf, &byte, 1);
}

void nu_buf_reset(nu_buf_t *buf) {
    if (!buf) return;
    buf->size = 0;
}

void nu_buf_free(nu_buf_t *buf) {
    if (!buf) return;
    if (buf->data) {
        nu_free(buf->mm, buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}
