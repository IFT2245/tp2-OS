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
#endif
