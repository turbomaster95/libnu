#include <config.h>
#include <nu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#ifdef HAVE_SYS_EPOLL_H
  #include <sys/epoll.h>
#else
  #error "libnu requires epoll to build!"
#endif

#ifdef HAVE_SYS_INOTIFY_H
  #include <sys/inotify.h>
#else
  #error "libnu requires inotify to build!"
#endif

#include <sys/timerfd.h>
#ifndef HAVE_TIMERFD_CREATE
   #error "libnu needs timerfd_create to build!"
#endif

#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <intnu.h>

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
    for (int i = 0; i < count; i++) free(list[i]);
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

char* nu_json_extract(const char *json, const char *key) {
    char target[128];
    snprintf(target, sizeof(target), "\"%s\"", key);
    const char *pos = strstr(json, target);
    if (!pos) return NULL;
    
    pos = strchr(pos + strlen(target), ':');
    if (!pos) return NULL;
    pos++;
    while (*pos && (isspace((unsigned char)*pos) || *pos == '"')) pos++;
    
    const char *end = pos;
    while (*end && *end != '"' && *end != ',' && *end != '}' && *end != ']') end++;
    
    size_t len = end - pos;
    if (len == 0) return NULL;
    
    char *res = malloc(len + 1);
    strncpy(res, pos, len);
    res[len] = '\0';
    return nu_str_trim(res);
}

nu_loop_t* nu_loop_create(void) {
    nu_loop_t *loop = malloc(sizeof(nu_loop_t));
    if (!loop) return NULL;
    loop->epoll_fd = epoll_create1(0);
    loop->inotify_fd = -1;
    loop->item_count = 0;
    return loop;
}

void nu_loop_destroy(nu_loop_t *loop) {
    if (!loop) return;
    close(loop->epoll_fd);
    if (loop->inotify_fd >= 0) close(loop->inotify_fd);
    for (int i = 0; i < loop->item_count; i++) free(loop->items[i]);
    free(loop);
}

bool nu_loop_add_fd(nu_loop_t *loop, int fd, nu_event_cb cb, void *data) {
    if (loop->item_count >= MAX_EVENTS) return false;
    
    nu_item_t *item = malloc(sizeof(nu_item_t));
    item->fd = fd; item->cb = cb; item->data = data; item->is_inotify = false;
    
    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = item };
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        free(item);
        return false;
    }
    loop->items[loop->item_count++] = item;
    return true;
}

bool nu_loop_add_timer(nu_loop_t *loop, int ms, nu_event_cb cb, void *data) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd < 0) return false;
    
    struct itimerspec ts = {
        .it_interval = { ms / 1000, (ms % 1000) * 1000000 },
        .it_value    = { ms / 1000, (ms % 1000) * 1000000 }
    };
    timerfd_settime(tfd, 0, &ts, NULL);
    return nu_loop_add_fd(loop, tfd, cb, data);
}

static void internal_inotify_handler(int fd, void *data) {
    nu_item_t *watch_item = (nu_item_t*)data;
    char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    ssize_t len = read(fd, buffer, sizeof(buffer));
    if (len <= 0) return;
    
    // Trigger consumer callback
    watch_item->cb(fd, watch_item->data);
}

bool nu_loop_add_watch(nu_loop_t *loop, const char *path, nu_event_cb cb, void *data) {
    if (loop->inotify_fd < 0) {
        loop->inotify_fd = inotify_init1(IN_NONBLOCK);
        if (loop->inotify_fd < 0) return false;
        // Watch the inotify file descriptor inside epoll
        nu_item_t *i_item = malloc(sizeof(nu_item_t));
        i_item->fd = loop->inotify_fd; i_item->cb = internal_inotify_handler; i_item->data = NULL;
        struct epoll_event ev = { .events = EPOLLIN, .data.ptr = i_item };
        epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, loop->inotify_fd, &ev);
    }
    
    int wd = inotify_add_watch(loop->inotify_fd, path, IN_MODIFY);
    if (wd < 0) return false;
    
    // Route notifications to user callback setup
    nu_item_t *user_item = malloc(sizeof(nu_item_t));
    user_item->fd = wd; user_item->cb = cb; user_item->data = data; user_item->is_inotify = true;
    
    // Attach custom event logic directly via internal lookup structures
    loop->items[loop->item_count++] = user_item;
    return true;
}

bool nu_loop_run(nu_loop_t *loop) {
    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int nfds = epoll_wait(loop->epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        for (int i = 0; i < nfds; i++) {
            nu_item_t *item = (nu_item_t*)events[i].data.ptr;
            if (item->fd == loop->inotify_fd) {
                // Handle inotify multiplexed streams
                char buf[4096];
                ssize_t len = read(item->fd, buf, sizeof(buf));
                if (len > 0) {
                    struct inotify_event *event = (struct inotify_event *)buf;
                    for (int j = 0; j < loop->item_count; j++) {
                        if (loop->items[j]->is_inotify && loop->items[j]->fd == event->wd) {
                            loop->items[j]->cb(event->wd, loop->items[j]->data);
                        }
                    }
                }
            } else {
                if (item->cb) {
                    // Consume timerfd tokens to avoid looping indefinitely
                    uint64_t missed;
                    read(item->fd, &missed, sizeof(missed)); 
                    item->cb(item->fd, item->data);
                }
            }
        }
    }
    return true;
}

void nu_ipc_listen(const char *sock_path, nu_ipc_cb cb, void *data) {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    
    unlink(sock_path);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 5) < 0) {
        close(server_fd);
        return;
    }

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        
        char buffer[1024] = {0};
        ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            cb(buffer, client_fd, data);
        }
        close(client_fd);
    }
    close(server_fd);
}

char* nu_ipc_send(const char *sock_path, const char *message) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return NULL;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }

    write(sock, message, strlen(message));
    
    char *response = malloc(2048);
    memset(response, 0, 2048);
    ssize_t n = read(sock, response, 2047);
    close(sock);

    if (n <= 0) {
        free(response);
        return NULL;
    }
    return response;
}
