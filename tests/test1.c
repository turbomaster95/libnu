#include <nu.h>
#include <stdint.h>
#include <stdlib.h>

nu_mm_t *g_mm = NULL;

char backing[32768];

void on_every_second(int fd, void *data) {
    NU_INFO("[TIMER] 1 Second elapsed!\n");
}

void on_config_modified(int fd, void *data) {
    NU_INFO("[WATCHER] Config file changed! Reloading variables...\n");
}

int main() {
    NU_INFO("--- testing libnu utils ---\n");

    g_mm = nu_mm_create(NU_MM_ARENA, backing, sizeof(backing));

    int count = 0;
    char **tags = nu_str_split("libnu,is,good,and,the,best", ",", &count);
    for(int i = 0; i < count; i++) {
        NU_INFO("Tag [%d]: %s\n", i, tags[i]);
    }
    nu_str_free_list(tags, count);

    NU_INFO("\nModify '/tmp/test.conf' to trigger watcher...\n");
    system("touch /tmp/test.conf");

    nu_loop_t *loop = nu_loop_create(g_mm);
    
    nu_loop_add_timer(loop, g_mm, 1000, on_every_second, NULL);
    nu_loop_add_watch(loop, g_mm, "/tmp/test.conf", on_config_modified, NULL);
    
    nu_loop_run(loop); // Starts ticking infinitely

    nu_loop_destroy(loop, g_mm);
    return 0;
}
