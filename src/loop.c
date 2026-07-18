#include <config.h>
#include <nu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <intnu.h>

#ifdef HAVE_SYS_EPOLL_H
  #include <sys/epoll.h>
#endif
#ifdef HAVE_SYS_INOTIFY_H
  #include <sys/inotify.h>
#endif
#ifdef HAVE_TIMERFD_CREATE
  #include <sys/timerfd.h>
#endif

#define MAX_EPOLL_EVENTS 64

#if defined(HAVE_SYS_EPOLL_H)

nu_loop_t* nu_loop_create(nu_mm_t *mm) {
    if (!mm) return NULL;
    nu_loop_t *loop = nu_alloc(mm, sizeof(nu_loop_t));
    if (!loop) return NULL;

    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd < 0) {
        nu_free(mm, loop);
        return NULL;
    }
    
    loop->inotify_fd = -1;
    
    loop->items_map = nu_map_create(mm, 16);
    if (!loop->items_map) {
        close(loop->epoll_fd);
        nu_free(mm, loop);
        return NULL;
    }
    
    return loop;
}

void nu_loop_destroy(nu_loop_t *loop, nu_mm_t *mm) {
    if (!loop) return;
    if (loop->epoll_fd >= 0) close(loop->epoll_fd);
    if (loop->inotify_fd >= 0) close(loop->inotify_fd);
    
    nu_map_destroy(loop->items_map);
    nu_free(mm, loop);
}

bool nu_loop_add_fd(nu_loop_t *loop, nu_mm_t *mm, int fd, nu_event_cb cb, void *data) {
    if (!mm) return false;
    nu_item_t *item = nu_alloc(mm, sizeof(nu_item_t));
    if (!item) return false;
    
    item->fd = fd; 
    item->cb = cb; 
    item->data = data; 
    item->is_inotify = false;
    
    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = item };
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        free(item);
        return false;
    }
    
    char key[32];
    nu_snprintf(key, sizeof(key), "%d", fd);
    nu_map_set(loop->items_map, key, item);
    return true;
}

#else

nu_loop_t* nu_loop_create(nu_mm_t *mm) { (void)mm; return NULL; }
void nu_loop_destroy(nu_loop_t *loop) { (void)loop; }
bool nu_loop_add_fd(nu_loop_t *loop, int fd, nu_event_cb cb, void *data) {
    (void)loop; (void)fd; (void)cb; (void)data; return false;
}

#endif

#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_TIMERFD_CREATE)

bool nu_loop_add_timer(nu_loop_t *loop, nu_mm_t *mm, int ms, nu_event_cb cb, void *data) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd < 0) return false;
    
    struct itimerspec ts = {
        .it_interval = { ms / 1000, (ms % 1000) * 1000000 },
        .it_value    = { ms / 1000, (ms % 1000) * 1000000 }
    };
    timerfd_settime(tfd, 0, &ts, NULL);
    return nu_loop_add_fd(loop, mm, tfd, cb, data);
}

#else

bool nu_loop_add_timer(nu_loop_t *loop, int ms, nu_event_cb cb, void *data) {
    (void)loop; (void)ms; (void)cb; (void)data; return false;
}

#endif

#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_SYS_INOTIFY_H)

static void internal_inotify_handler(int fd, void *data) {
    (void)data;
}

bool nu_loop_add_watch(nu_loop_t *loop, nu_mm_t *mm, const char *path, nu_event_cb cb, void *data) {
    if (loop->inotify_fd < 0) {
        loop->inotify_fd = inotify_init1(IN_NONBLOCK);
        if (loop->inotify_fd < 0) return false;
        if (!mm) return false;

        nu_item_t *i_item = nu_alloc(mm, sizeof(nu_item_t));
        if (!i_item) return false;
        i_item->fd = loop->inotify_fd; 
        i_item->cb = internal_inotify_handler; 
        i_item->data = NULL;
        
        struct epoll_event ev = { .events = EPOLLIN, .data.ptr = i_item };
        epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, loop->inotify_fd, &ev);
    }
    
    int wd = inotify_add_watch(loop->inotify_fd, path, IN_MODIFY);
    if (wd < 0) return false;
    
    if (!mm) return false;
    nu_item_t *user_item = nu_alloc(mm, sizeof(nu_item_t));
    if (!user_item) return false;
    user_item->fd = wd; 
    user_item->cb = cb; 
    user_item->data = data; 
    user_item->is_inotify = true;
    
    char key[32];
    nu_snprintf(key, sizeof(key), "wd_%d", wd);
    nu_map_set(loop->items_map, key, user_item);
    return true;
}

#else

bool nu_loop_add_watch(nu_loop_t *loop, const char *path, nu_event_cb cb, void *data) {
    (void)loop; (void)path; (void)cb; (void)data; return false;
}

#endif

#if defined(HAVE_SYS_EPOLL_H)

bool nu_loop_run(nu_loop_t *loop) {
    struct epoll_event events[MAX_EPOLL_EVENTS];
    while (1) {
        int nfds = epoll_wait(loop->epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        for (int i = 0; i < nfds; i++) {
            nu_item_t *item = (nu_item_t*)events[i].data.ptr;
            
#if defined(HAVE_SYS_INOTIFY_H)
            if (item->fd == loop->inotify_fd) {
                char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
                ssize_t len = read(item->fd, buf, sizeof(buf));
                if (len > 0) {
                    ssize_t offset = 0;
                    while (offset < len) {
                        struct inotify_event *event = (struct inotify_event *)&buf[offset];
                        char key[32];
                        nu_snprintf(key, sizeof(key), "wd_%d", event->wd);
                        
                        nu_item_t *watch_item = (nu_item_t*)nu_map_get(loop->items_map, key);
                        if (watch_item && watch_item->cb) {
                            watch_item->cb(event->wd, watch_item->data);
                        }
                        offset += sizeof(struct inotify_event) + event->len;
                    }
                }
                continue;
            }
#endif
            
            if (item->cb) {
#if defined(HAVE_TIMERFD_CREATE)
                uint64_t missed;
                ssize_t r = read(item->fd, &missed, sizeof(missed)); 
                (void)r;
#endif
                item->cb(item->fd, item->data);
            }
        }
    }
    return true;
}

#else

bool nu_loop_run(nu_loop_t *loop) { (void)loop; return false; }

#endif
