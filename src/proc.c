#include <config.h>
#include <nu.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <intnu.h>

typedef struct {
    nu_proc_io_cb io_cb;
    void *user_data;
} nu_proc_wrapper_t;

static void internal_proc_handler(int fd, void *data) {
    nu_proc_wrapper_t *wrapper = (nu_proc_wrapper_t*)data;
    char buf[512];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    
    if (n > 0) {
        buf[n] = '\0';
        if (wrapper->io_cb) wrapper->io_cb(buf, (size_t)n, wrapper->user_data);
    } else {
        // EOF or Pipe Closed: clean up structures
        close(fd);
        free(wrapper);
        // Note: Clean up from loop tracking items array contextually if necessary
    }
}

bool nu_process_spawn(nu_loop_t *loop, char *const argv[], nu_proc_io_cb stdout_cb, void *data) {
    int p_fds[2];
    if (pipe(p_fds) < 0) return false;

    // Set read end to non-blocking
    fcntl(p_fds[0], F_SETFL, O_NONBLOCK);

    pid_t pid = fork();
    if (pid < 0) {
        close(p_fds[0]); close(p_fds[1]);
        return false;
    }

    if (pid == 0) { // Child pipeline execution
        close(p_fds[0]);
        dup2(p_fds[1], STDOUT_FILENO);
        dup2(p_fds[1], STDERR_FILENO);
        close(p_fds[1]);

        execvp(argv[0], argv);
        exit(127); // Fail escape
    }

    // Parent
    close(p_fds[1]);

    nu_proc_wrapper_t *wrapper = malloc(sizeof(nu_proc_wrapper_t));
    wrapper->io_cb = stdout_cb;
    wrapper->user_data = data;

    return nu_loop_add_fd(loop, p_fds[0], internal_proc_handler, wrapper);
}
