#ifndef TEST_COMMON_H
#define TEST_COMMON_H
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern char g_test_fail_reason[256];
struct captured_output{ char stdout_buf[8192]; char stderr_buf[8192]; };

int run_function_capture_output(void(*fn)(void), struct captured_output* out);

#define TEST(name) static bool test_##name(void)
#define RUN_TEST(name) do{ \
bool ok=test_##name(); \
tests_run++; \
if(!ok){ \
tests_failed++; \
printf("  FAIL: %s => %s\n",#name,g_test_fail_reason[0]?g_test_fail_reason:"???"); \
} else { \
printf("  PASS: %s\n",#name); \
} \
}while(0)

#endif
