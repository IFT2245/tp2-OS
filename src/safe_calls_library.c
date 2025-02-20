#include "safe_calls_library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <float.h>

int parse_int_strtol(const char* input, int fallback) {
    if (!input || !*input) {
        return fallback;
    }
    errno = 0;
    char* endptr = NULL;
    long val = strtol(input, &endptr, 10);
    if (endptr == input || errno == ERANGE || val < INT_MIN || val > INT_MAX) {
        return fallback;
    }
    return (int)val;
}

long parse_long_strtol(const char* input, long fallback) {
    if (!input || !*input) {
        return fallback;
    }
    errno = 0;
    char* endptr = NULL;
    long val = strtol(input, &endptr, 10);
    if (endptr == input || errno == ERANGE) {
        return fallback;
    }
    return val;
}

float parse_float_strtof(const char* input, float fallback) {
    if (!input || !*input) {
        return fallback;
    }
    errno = 0;
    char* endptr = NULL;
    float val = strtof(input, &endptr);
    if (endptr == input || errno == ERANGE) {
        return fallback;
    }
    return val;
}

double parse_double_strtod(const char* input, double fallback) {
    if (!input || !*input) {
        return fallback;
    }
    errno = 0;
    char* endptr = NULL;
    double val = strtod(input, &endptr);
    if (endptr == input || errno == ERANGE) {
        return fallback;
    }
    return val;
}
