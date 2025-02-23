#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../src/safe_calls_library.h"

/*
  A global buffer for test failure reason.
*/
extern char g_test_fail_reason[256];

/*
  Structure to hold stdout/stderr from a forked child test run.
*/
struct captured_output {
  char stdout_buf[8192];
  char stderr_buf[8192];
};

/*
  Runs the given function 'fn' in a child process,
  capturing the child's stdout/stderr into 'out'.
  Returns the child's exit code.
*/
int run_function_capture_output(void(*fn)(void), struct captured_output* out);

/*
  TEST macro => define a test function that returns bool.
  RUN_TEST => run that function, track pass/fail, print result.
*/
#define TEST(name) static bool test_##name(void)
#define RUN_TEST(name) do { \
bool ok=test_##name(); \
tests_run++; \
if(!ok){ \
tests_failed++; \
printf("  FAIL: %s => %s\n",#name, \
g_test_fail_reason[0]?g_test_fail_reason:"???"); \
} else { \
printf("  PASS: %s\n", #name); \
} \
} while(0)

#endif
