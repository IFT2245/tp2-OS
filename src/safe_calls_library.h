#ifndef SAFE_CALLS_LIBRARY_H
#define SAFE_CALLS_LIBRARY_H
#include <stddef.h>
#include <signal.h>
#include "os.h"
#include "runner.h"
#include "scoreboard.h"
#include "stats.h"
#include "menu.h"
#include "limits.h"

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"
#define CLR_RED     "\033[91m"
#define CLR_GREEN   "\033[92m"
#define CLR_GRAY    "\033[90m"
#define CLR_YELLOW  "\033[93m"
#define CLR_CYAN    "\033[96m"

/* parse integer via strtol; returns fallback if invalid */
int    parse_int_strtol(const char* input, int fallback);

/* parse long via strtol; returns fallback if invalid */
long   parse_long_strtol(const char* input, long fallback);

/* parse float via strtof; returns fallback if invalid */
float  parse_float_strtof(const char* input, float fallback);

/* parse double via strtod; returns fallback if invalid */
double parse_double_strtod(const char* input, double fallback);

/* Cleanup function that saves scoreboard, prints stats, and exits the program. */
void cleanup_and_exit(int code);

/* Signal handler for SIGINT, SIGTERM, etc. */
void handle_signal(int signum);
#endif
