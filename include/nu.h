#ifndef LIBNU_H
#define LIBNU_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    NU_LOG_DEBUG,
    NU_LOG_INFO,
    NU_LOG_WARN,
    NU_LOG_ERROR
} nu_log_level_t;

typedef void (*nu_proc_io_cb)(const char *buffer, size_t len, void *data);

void nu_log_output(nu_log_level_t level, const char *file, int line, const char *fmt, ...);

#define NU_DEBUG(fmt, ...) nu_log_output(NU_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_INFO(fmt, ...)  nu_log_output(NU_LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_WARN(fmt, ...)  nu_log_output(NU_LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_ERROR(fmt, ...) nu_log_output(NU_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Memory-safe string split. Returns heap-allocated array of strings. Free with nu_str_free_list.
char** nu_str_split(const char *str, const char *delim, int *out_count);
void   nu_str_free_list(char **list, int count);

// Trims whitespace in-place (modifies the buffer)
char* nu_str_trim(char *str);

// Quick JSON string extractor. Extracts value matching a top-level or dotted key.
// Caller must free() returned string. Returns NULL if not found.
char* nu_json_extract(const char *json, const char *key);

typedef struct nu_loop nu_loop_t;
typedef void (*nu_event_cb)(int fd, void *data);

nu_loop_t* nu_loop_create(void);
void       nu_loop_destroy(nu_loop_t *loop);

// Add a standard file descriptor to watch for readability
bool       nu_loop_add_fd(nu_loop_t *loop, int fd, nu_event_cb cb, void *data);

// Add a timer (interval in milliseconds)
bool       nu_loop_add_timer(nu_loop_t *loop, int ms, nu_event_cb cb, void *data);

// Watch a file/directory path for modifications
bool       nu_loop_add_watch(nu_loop_t *loop, const char *path, nu_event_cb cb, void *data);

// Run the loop indefinitely. Returns false on catastrophic failure.
bool       nu_loop_run(nu_loop_t *loop);

typedef void (*nu_ipc_cb)(const char *msg, int client_fd, void *data);

// Starts a blocking IPC server. Typically spawned in its own thread or used as a standalone daemon.
// Automatically creates a UNIX socket at 'sock_path'.
void       nu_ipc_listen(const char *sock_path, nu_ipc_cb cb, void *data);

// Sends a quick message to a listening IPC server and returns response (or NULL)
char* nu_ipc_send(const char *sock_path, const char *message);

// Mask and monitor system signals via epoll loop.
// Pass signum (e.g., SIGINT, SIGTERM). Returns false on failure.
bool nu_loop_add_signal(nu_loop_t *loop, int signum, nu_event_cb cb, void *data);

// Exec background processes without hanging. Pipeline matches standard argv format.
bool nu_process_spawn(nu_loop_t *loop, char *const argv[], nu_proc_io_cb stdout_cb, void *data);

#endif // LIBNU_H
