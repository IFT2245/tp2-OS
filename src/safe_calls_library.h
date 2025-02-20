#ifndef SAFE_CALLS_LIBRARY_H
#define SAFE_CALLS_LIBRARY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * parse_int_strtol: safer alternative to atoi,
     * logs or discards invalid input, returns fallback on error.
     */
    int parse_int_strtol(const char* input, int fallback);

    /*
     * parse_long_strtol: parse a long integer safely.
     */
    long parse_long_strtol(const char* input, long fallback);

    /*
     * parse_float_strtof: parse a float with error checks.
     */
    float parse_float_strtof(const char* input, float fallback);

    /*
     * parse_double_strtod: parse double with error checks.
     */
    double parse_double_strtod(const char* input, double fallback);

#ifdef __cplusplus
}
#endif

#endif /* SAFE_CALLS_LIBRARY_H */
