#ifndef NU_INTERNAL_H
#define NU_INTERNAL_H

#include <nu.h>
#include <sys/epoll.h>
#include <stdbool.h>

#define MAX_EVENTS 64

typedef struct {
    int fd;
    nu_event_cb cb;
    void *data;
    bool is_inotify;
} nu_item_t;

struct nu_loop {
    int epoll_fd;
    int inotify_fd;
    nu_item_t *items[MAX_EVENTS];
    int item_count;
};

#endif // NU_INTERNAL_H

