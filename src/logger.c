// Created by spokboud
#include "logger.h"
#include <stdio.h>


log_level_t g_log_level = LOG_LEVEL_INFO;

void set_log_level(log_level_t lvl){ g_log_level = lvl; }
void vlogf(log_level_t lvl,const char* prefix,const char* fmt,va_list ap){
    if(lvl<g_log_level)return;
    fprintf(stderr,"%s",prefix);
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"\n");
}
void log_debug(const char* fmt, ...){
    va_list ap;va_start(ap,fmt);vlogf(LOG_LEVEL_DEBUG,"[DEBUG] ",fmt,ap);va_end(ap);
}

void log_info(const char* fmt, ...){
    va_list ap;va_start(ap,fmt);vlogf(LOG_LEVEL_INFO,"[INFO]  ",fmt,ap);va_end(ap);
}

void log_warn(const char* fmt, ...){
    va_list ap;va_start(ap,fmt);vlogf(LOG_LEVEL_WARN,"[WARN]  ",fmt,ap);va_end(ap);
}

void log_error(const char* fmt, ...){
    va_list ap;va_start(ap,fmt);vlogf(LOG_LEVEL_ERROR,"[ERROR] ",fmt,ap);va_end(ap);
}