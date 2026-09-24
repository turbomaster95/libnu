#include <config.h>
#include <nu.h>
#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <intnu.h>

void nu_log_output(nu_log_level_t level, const char *file, int line, const char *fmt, ...) {
    time_t raw;
    time(&raw);
    struct tm *timeinfo = localtime(&raw);
    
    char t_str[9];
    strftime(t_str, sizeof(t_str), "%H:%M:%S", timeinfo);

    const char *lbl;
    const char *col;
    switch(level) {
        case NU_LOG_DEBUG: lbl = "DBG"; col = "\033[36m"; break; // Cyan
        case NU_LOG_INFO:  lbl = "INF"; col = "\033[32m"; break; // Green
        case NU_LOG_WARN:  lbl = "WRN"; col = "\033[33m"; break; // Yellow
        case NU_LOG_ERROR: lbl = "ERR"; col = "\033[31m"; break; // Red
        default: return;
    }

    fprintf(stderr, "%s %s[%s]\033[0m (%s:%d) ", t_str, col, lbl, file, line);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

