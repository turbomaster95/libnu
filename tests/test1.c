#include <nu.h>
#include <stdio.h>
#include <stdlib.h>

void on_every_second(int fd, void *data) {
    printf("[TIMER] 1 Second elapsed!\n");
}

void on_config_modified(int fd, void *data) {
    printf("[WATCHER] Config file changed! Reloading variables...\n");
}

int main() {
    printf("--- testing libnu utils ---\n");

    const char *api_payload = "{\"status\": \"active\", \"battery\": \"89%\"}";
    char *bat = nu_json_extract(api_payload, "battery");
    printf("Extracted JSON Property: %s\n", bat); // Prints 89%
    free(bat);

    int count = 0;
    char **tags = nu_str_split("libnu,is,good,and,the,best", ",", &count);
    for(int i = 0; i < count; i++) {
        printf("Tag [%d]: %s\n", i, tags[i]);
    }
    nu_str_free_list(tags, count);

    printf("\nModify '/tmp/test.conf' to trigger watcher...\n");
    system("touch /tmp/test.conf");

    nu_loop_t *loop = nu_loop_create();
    
    nu_loop_add_timer(loop, 1000, on_every_second, NULL);
    nu_loop_add_watch(loop, "/tmp/test.conf", on_config_modified, NULL);
    
    nu_loop_run(loop); // Starts ticking infinitely

    nu_loop_destroy(loop);
    return 0;
}
