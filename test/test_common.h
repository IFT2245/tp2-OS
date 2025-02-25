#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdbool.h>

/* Provide a macro for test definitions and a reason buffer. */
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

#define TEST(name) static bool test_##name(void)

/* RUN_TEST macro to unify pass/fail printing. */
#define RUN_TEST(name) do {                                    \
bool ok = test_##name();                                   \
tests_run++;                                               \
if(!ok){                                                   \
tests_failed++;                                        \
printf("  FAIL: %s => %s\n", #name, test_get_fail_reason()); \
} else {                                                   \
printf("  PASS: %s\n", #name);                         \
}                                                          \
} while(0)

#endif
