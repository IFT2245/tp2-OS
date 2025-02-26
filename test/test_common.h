#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Colors for ASCII art convenience. */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"
#define CLR_RED     "\033[91m"
#define CLR_GREEN   "\033[92m"
#define CLR_GRAY    "\033[90m"
#define CLR_YELLOW  "\033[93m"
#define CLR_CYAN    "\033[96m"

void test_set_fail_reason(const char* msg);
const char* test_get_fail_reason(void);

struct captured_output {
  char stdout_buf[8192];
  char stderr_buf[8192];
};

/*
  run_function_capture_output():
    Creates pipes, forks, runs fn() in child, captures output in out.
*/
int run_function_capture_output(void(*fn)(void), struct captured_output* out);

/* A macro for test function definitions. */
#define TEST(name) static bool test_##name(void)

/* RUN_TEST macro to unify pass/fail printing. */
#define RUN_TEST(name) do {                          \
bool ok = test_##name();                             \
tests_run++;                                         \
if(!ok){                                             \
tests_failed++;                                      \
printf("  %s " CLR_GRAY "%s" CLR_RESET "%s%s%s\n",   \
test_failed ? "✘" : "✔",                             \
#name,                                               \
test_failed ? " => " CLR_RED : "",                   \
test_failed ? test_get_fail_reason() : "",           \
test_failed ? CLR_RESET : "");                       \
}                                                    \
} while(0)

#endif
