#include <config.h>
#include <nu.h>

#ifdef HAVE_SYS_SIGNALFD_H
  #include <sys/signalfd.h>
#else
  #error "libnu needs sys/signalfd.h to exist!"
#endif

#ifdef HAVE_SIGNALFD
  #include <signal.h>
#else
  #error "libnu needs signal.h to exist!"
#endif

#ifdef HAVE_SYS_EPOLL_H
  #include <sys/epoll.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <intnu.h>

static void internal_signal_handler(int fd, void *data) {
    struct signalfd_siginfo fdsi;
    ssize_t s = read(fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) return;

    nu_item_t *sig_item = (nu_item_t*)data;
    if (sig_item && sig_item->cb) {
        sig_item->cb(fdsi.ssi_signo, sig_item->data);
    }
}

bool nu_loop_add_signal(nu_loop_t *loop, nu_mm_t *mm, int signum, nu_event_cb cb, void *data) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signum);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) return false;

    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd < 0) return false;

    if (!mm) return false;
    nu_item_t *sig_item = nu_alloc(mm, sizeof(nu_item_t));
    if (!sig_item) {
        close(sfd);
        return false;
    }
    sig_item->fd = sfd;
    sig_item->cb = cb;
    sig_item->data = data;
    sig_item->is_inotify = false;

    nu_item_t *epoll_hook = nu_alloc(mm, sizeof(nu_item_t));
    if (!epoll_hook) {
        nu_free(mm, sig_item);
        close(sfd);
        return false;
    }
    epoll_hook->fd = sfd;
    epoll_hook->cb = internal_signal_handler;
    epoll_hook->data = sig_item;
    epoll_hook->is_inotify = false;

    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = epoll_hook };

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, sfd, &ev) < 0) {
        nu_free(mm, epoll_hook);
        nu_free(mm, sig_item);
        close(sfd);
        return false;
    }
    
    char key[32];
    nu_snprintf(key, sizeof(key), "%d", sfd);
    nu_map_set(loop->items_map, key, epoll_hook);
    
    char wrapper_key[48];
    nu_snprintf(wrapper_key, sizeof(wrapper_key), "sig_data_%d", sfd);
    nu_map_set(loop->items_map, wrapper_key, sig_item);
    
    return true;
}
