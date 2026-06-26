#include <nu.h>
#include <stdio.h>
#include <stdlib.h>

#define SOCKET_PATH "/tmp/nu_test_daemon.sock"

int main() {
    NU_INFO("--- Running libnu Client Subsystem Test ---");

    char dirty_string[] = "   \t  Hello libnu Community!   \n\r ";
    NU_INFO("Before trim: '%s'", dirty_string);
    char *clean_string = nu_str_trim(dirty_string);
    NU_INFO("After trim:  '%s'", clean_string);

    NU_INFO("Sending 'ping' payload to daemon socket...");
    
    char *response1 = nu_ipc_send(SOCKET_PATH, "ping");
    if (response1) {
        NU_INFO("Daemon response received: %s", response1);
        free(response1);
    } else {
        NU_ERROR("Could not communicate with daemon. Is test2_daemon running?");
    }

    NU_INFO("Sending 'sysinfo' task command to daemon...");
    char *response2 = nu_ipc_send(SOCKET_PATH, "sysinfo");
    if (response2) {
        NU_INFO("Daemon response received: %s", response2);
        free(response2);
    }

    return 0;
}
