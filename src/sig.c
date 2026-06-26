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
    if (sig_item->cb) {
        sig_item->cb(fdsi.ssi_signo, sig_item->data);
    }
}

bool nu_loop_add_signal(nu_loop_t *loop, int signum, nu_event_cb cb, void *data) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signum);

    // Block traditional signal handling so it directs to the descriptor
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) return false;

    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd < 0) return false;

    // Route event directly inside our standard loop structures
    nu_item_t *sig_item = malloc(sizeof(nu_item_t));
    sig_item->fd = sfd;
    sig_item->cb = cb;
    sig_item->data = data;
    sig_item->is_inotify = false;

    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = sig_item };
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, sfd, &ev) < 0) {
        free(sig_item);
        close(sfd);
        return false;
    }
    
    loop->items[loop->item_count++] = sig_item;
    return true;
}

