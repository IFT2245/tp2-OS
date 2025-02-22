#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static char g_test_fail_reason[256]={0};

struct captured_output {
    char stdout_buf[4096];
    char stderr_buf[4096];
};

int run_function_capture_output(void(*fn)(void), struct captured_output* out);
int run_shell_command_capture_output(const char* line, struct captured_output* out);
int run_command_capture_output(char* const argv[], struct captured_output* out);

static inline void test_unicode_pass(const char* test_name){
    printf("  ✅  %s => reason: logs OK.\n", test_name);
}

static inline void test_unicode_fail(const char* test_name, const char* reason){
    printf("  ❌  %s => reason: %s\n", test_name, reason);
}

#define TEST(name) static bool test_##name(void)

/*
   We clear g_test_fail_reason, run the test, if fails => print reason or 'no reason set'.
*/
#define RUN_TEST(name) do {                                          \
memset(g_test_fail_reason,0,sizeof(g_test_fail_reason));          \
bool result=test_##name();                                       \
tests_run++;                                                     \
if(!result){                                                     \
tests_failed++;                                              \
test_unicode_fail(#name, g_test_fail_reason[0]?g_test_fail_reason:"no reason set"); \
} else {                                                         \
test_unicode_pass(#name);                                    \
}                                                                \
} while(0)

#endif
