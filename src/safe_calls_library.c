#include "safe_calls_library.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

int parse_int_strtol(const char* in, int fb) {
    if(!in || !*in) return fb;
    errno=0;
    char* e=NULL;
    long v = strtol(in, &e, 10);
    if(e==in || errno==ERANGE || v<INT_MIN || v>INT_MAX) return fb;
    return (int)v;
}

long parse_long_strtol(const char* in, long fb) {
    if(!in || !*in) return fb;
    errno=0;
    char* e=NULL;
    long v = strtol(in, &e, 10);
    if(e==in || errno==ERANGE) return fb;
    return v;
}

float parse_float_strtof(const char* in, float fb) {
    if(!in || !*in) return fb;
    errno=0;
    char* e=NULL;
    float v = strtof(in, &e);
    if(e==in || errno==ERANGE) return fb;
    return v;
}

double parse_double_strtod(const char* in, double fb) {
    if(!in || !*in) return fb;
    errno=0;
    char* e=NULL;
    double v = strtod(in, &e);
    if(e==in || errno==ERANGE) return fb;
    return v;
}
