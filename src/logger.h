#ifndef LOGGER_H
#define LOGGER_H
#include <stdarg.h>

typedef enum {
    LOG_LEVEL_DEBUG=0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
  } log_level_t;

void set_log_level(log_level_t lvl);
void vlogf(log_level_t lvl,const char* prefix,const char* fmt,va_list ap);
void log_debug(const char* fmt, ...);
void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_error(const char* fmt, ...);

#endif //LOGGER_H
