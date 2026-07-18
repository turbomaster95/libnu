#include <nu.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

void nu_ipc_listen(nu_mm_t *mm, const char *sock_path, nu_ipc_cb cb, void *data) {
    if (!mm) return;
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

char* nu_ipc_send(nu_mm_t *mm, const char *sock_path, const char *message) {
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
    
    char *response = nu_alloc(mm, 2048);
    if (!response) {
        close(sock);
        return NULL;
    }
    memset(response, 0, 2048);
    ssize_t n = read(sock, response, 2047);
    close(sock);

    if (n <= 0) {
        nu_free(mm, response);
        return NULL;
    }
    return response;
}
