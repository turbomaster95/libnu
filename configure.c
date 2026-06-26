#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <ctype.h>

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

// In-place whitespace trim helper
char* trim_space(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int main() {
    char cc[64] = "gcc";
    char cflags[256] = "-Wall -O2 -fPIC";
    char includes[256] = "-I./include -I.";
    char lib_src_raw[512] = "src/main.c";
    char target_so[64] = "libnu.so";
    char target_a[64] = "libnu.a";

    FILE *f = fopen("bld.txt", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = trim_space(line); 
            char *val = trim_space(eq + 1);
            if (strcmp(key, "CC") == 0) strcpy(cc, val);
            else if (strcmp(key, "CFLAGS") == 0) strcpy(cflags, val);
            else if (strcmp(key, "INCLUDES") == 0) strcpy(includes, val);
            else if (strcmp(key, "SOURCES") == 0) strcpy(lib_src_raw, val);
            else if (strcmp(key, "TARGET_SO") == 0) strcpy(target_so, val);
            else if (strcmp(key, "TARGET_A") == 0) strcpy(target_a, val);
        }
        fclose(f);
    }

    printf("--- libnu Configurer ---\n");

    FILE *config_h = fopen("include/config.h", "w");
    if (!config_h) return 1;
    fprintf(config_h, "#ifndef CONFIG_H\n#define CONFIG_H\n\n");

    f = fopen("bld.txt", "r");
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
            char *macro_name = trim_space(line);
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

    // Track list of all compiled object files for the link stage
    char object_files_list[1024] = "";
    
    // Parse individual files from comma-separated SOURCES entry
    char *token = strtok(lib_src_raw, ",");
    while (token != NULL) {
        char *src_file = trim_space(token);
        if (strlen(src_file) > 0) {
            char obj_file[256];
            get_obj_name(src_file, obj_file, sizeof(obj_file));
            
            // Output the discrete Ninja compile target
            fprintf(ninja, "build %s: compile %s\n", obj_file, src_file);
            
            // Append target object to build list
            strcat(object_files_list, obj_file);
            strcat(object_files_list, " ");
        }
        token = strtok(NULL, ",");
    }
    
    // Trim trailing space from object list
    char *trimmed_obj_list = trim_space(object_files_list);

    fprintf(ninja, "\nbuild %s: link_shared %s\n", target_so, trimmed_obj_list);
    fprintf(ninja, "build %s: link_static %s\n\n", target_a, trimmed_obj_list);
    fprintf(ninja, "default %s %s\n", target_so, target_a);

    fclose(ninja);
    return 0;
}

