#include <nu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/nu_test_daemon.sock"

nu_loop_t *global_loop = NULL;

void on_process_stdout(const char *buffer, size_t len, void *data) {
    NU_INFO("[DAEMON SPARK] Process output received (%zu bytes): %.*s", len, (int)len, buffer);
}

void on_signal_received(int fd, void *data) {
    NU_WARN("[DAEMON] Shutdown signal intercepted! Cleaning up and exiting loop...");
    unlink(SOCKET_PATH);
    exit(0);
}

void on_ipc_message(const char *msg, int client_fd, void *data) {
    NU_INFO("[DAEMON IPC] Received command: %s", msg);

    if (strcmp(msg, "ping") == 0) {
        dprintf(client_fd, "pong");
    } 
    else if (strcmp(msg, "sysinfo") == 0) {
        char *const argv[] = {"uname", "-a", NULL};
        NU_INFO("[DAEMON IPC] Spawning background 'uname' command...");
        nu_process_spawn(global_loop, argv, on_process_stdout, NULL);
        dprintf(client_fd, "Spawning sysinfo command in background. Check daemon log.");
    } 
    else {
        dprintf(client_fd, "Unknown command string");
    }
}

int main() {
    printf("Starting libnu test daemon...\n");
    printf("This will detach into the background. Track logs using system log or your custom logger.\n");

    if (!nu_daemonize()) {
        NU_ERROR("Daemonization failed!");
        return 1;
    }

    NU_INFO("Daemon successfully initialized with PID: %d", getpid());

    global_loop = nu_loop_create();
    if (!global_loop) {
        NU_ERROR("Failed to create loop container");
        return 1;
    }

    nu_loop_add_signal(global_loop, SIGINT, on_signal_received, NULL);
    nu_loop_add_signal(global_loop, SIGTERM, on_signal_received, NULL);

    NU_INFO("Listening for IPC requests at %s...", SOCKET_PATH);
    unlink(SOCKET_PATH);

    nu_ipc_listen(SOCKET_PATH, on_ipc_message, NULL);

    nu_loop_destroy(global_loop);
    return 0;
}
