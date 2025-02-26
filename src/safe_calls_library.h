#ifndef SAFE_CALLS_LIBRARY_H
#define SAFE_CALLS_LIBRARY_H

#include <stddef.h>

/*
 * Safe calls: parse integers/floats with fallback on error.
 */

/* parse integer via strtol; returns fallback if invalid */
int    parse_int_strtol(const char* input, int fallback);

/* parse long via strtol; returns fallback if invalid */
long   parse_long_strtol(const char* input, long fallback);

/* parse float via strtof; returns fallback if invalid */
float  parse_float_strtof(const char* input, float fallback);

/* parse double via strtod; returns fallback if invalid */
double parse_double_strtod(const char* input, double fallback);

#endif
