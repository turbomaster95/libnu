#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

#define TEMP_SRC "build_probe.c"
#define TEMP_BIN "build_probe.bin"

bool try_compile(const char *compiler, const char *source_code) {
    FILE *f = fopen(TEMP_SRC, "w");
    if (!f) return false;
    fprintf(f, "%s", source_code);
    fclose(f);
    pid_t pid = fork();
    if (pid == 0) {
        FILE *dev_null = fopen("/dev/null", "w");
        if (dev_null) { dup2(fileno(dev_null), STDERR_FILENO); fclose(dev_null); }
        char *args[] = {(char *)compiler, TEMP_SRC, "-o", TEMP_BIN, NULL};
        execvp(compiler, args);
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    unlink(TEMP_SRC);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) { unlink(TEMP_BIN); return true; }
    return false;
}

void get_obj_name(const char *src, char *obj, size_t max_len) {
    strncpy(obj, src, max_len - 1);
    obj[max_len - 1] = '\0';
    char *dot = strrchr(obj, '.');
    if (dot) strcpy(dot, ".o");
}

// Utility to replace "\n" strings with literal newline characters
void expand_newlines(char *dst, const char *src) {
    while (*src) {
        if (*src == '\\' && *(src + 1) == 'n') {
            *dst++ = '\n';
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

int main() {
    char cc[64] = "gcc";
    char cflags[256] = "-Wall -O2 -fPIC";
    char includes[256] = "-I./include -I.";
    char lib_src[256] = "src/main.c";
    char target_so[64] = "libnu.so";
    char target_a[64] = "libnu.a";

    FILE *f = fopen("info.txt", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = line; char *val = eq + 1;
            if (strcmp(key, "CC") == 0) strcpy(cc, val);
            else if (strcmp(key, "CFLAGS") == 0) strcpy(cflags, val);
            else if (strcmp(key, "INCLUDES") == 0) strcpy(includes, val);
            else if (strcmp(key, "SOURCES") == 0) strcpy(lib_src, val);
            else if (strcmp(key, "TARGET_SO") == 0) strcpy(target_so, val);
            else if (strcmp(key, "TARGET_A") == 0) strcpy(target_a, val);
        }
        fclose(f);
    }

    printf("--- libnu Configurer ---\n");

    FILE *config_h = fopen("config.h", "w");
    if (!config_h) return 1;
    fprintf(config_h, "#ifndef CONFIG_H\n#define CONFIG_H\n\n");

    f = fopen("info.txt", "r");
    if (f) {
        char line[512];
        bool in_probe_block = false;

        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (strcmp(line, "[probes]") == 0) {
                in_probe_block = true;
                continue;
            } else if (line[0] == '[') {
                in_probe_block = false;
            }

            char *eq = strchr(line, '=');
            if (!eq || !in_probe_block) continue;
            *eq = '\0';
            char *macro_name = line;
            char *raw_code = eq + 1;

            char clean_code[1024];
            expand_newlines(clean_code, raw_code);

            printf("Running probe for %s... ", macro_name);
            if (try_compile(cc, clean_code)) {
                printf("found\n");
                fprintf(config_h, "#define %s 1\n", macro_name);
            } else {
                printf("not found\n");
            }
        }
        fclose(f);
    }

    fprintf(config_h, "\n#endif\n");
    fclose(config_h);

    FILE *ninja = fopen("build.ninja", "w");
    if (!ninja) return 1;

    fprintf(ninja, "# Generated dynamically by configure.c\n\n");
    fprintf(ninja, "cc = %s\n", cc);
    fprintf(ninja, "cflags = %s\n", cflags);
    fprintf(ninja, "includes = %s\n\n", includes);

    fprintf(ninja, "rule compile\n  command = $cc $cflags $includes -c $in -o $out\n\n");
    fprintf(ninja, "rule link_shared\n  command = $cc -shared -o $out $in\n\n");
    fprintf(ninja, "rule link_static\n  command = ar rcs $out $in\n\n");

    char lib_obj[256];
    get_obj_name(lib_src, lib_obj, sizeof(lib_obj));
    fprintf(ninja, "build %s: compile %s\n", lib_obj, lib_src);
    fprintf(ninja, "build %s: link_shared %s\n", target_so, lib_obj);
    fprintf(ninja, "build %s: link_static %s\n\n", target_a, lib_obj);
    fprintf(ninja, "default %s %s\n", target_so, target_a);

    fclose(ninja);
    return 0;
}
