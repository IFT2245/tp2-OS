#include "log.h"

static log_level_t g_log_level = LOG_LEVEL_INFO;

static void vlogf(log_level_t lvl, const char* prefix, const char* fmt, va_list ap){
    if(lvl < g_log_level) return;
    fprintf(stderr, "%s", prefix);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, CLR_RESET "\n");
}

void set_log_level(log_level_t lvl){
    g_log_level = lvl;
}

void log_debug(const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vlogf(LOG_LEVEL_DEBUG, CLR_BLUE "[DEBUG] " CLR_RESET, fmt, ap);
    va_end(ap);
}

void log_info(const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vlogf(LOG_LEVEL_INFO, CLR_GREEN "[INFO]  " CLR_RESET, fmt, ap);
    va_end(ap);
    fflush(stdout);
    fflush(stderr);
}

void log_warn(const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vlogf(LOG_LEVEL_WARN, CLR_YELLOW "[WARN]  " CLR_RESET, fmt, ap);
    va_end(ap);
}

void log_error(const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vlogf(LOG_LEVEL_ERROR, CLR_RED "[ERROR] " CLR_RESET, fmt, ap);
    va_end(ap);
}
