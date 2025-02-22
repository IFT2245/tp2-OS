#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* We'll capture stdout/stderr in buffers for validation. */
struct captured_output {
    char stdout_buf[4096];
    char stderr_buf[4096];
};

/* Runs 'fn' in a child process, capturing its stdout/stderr. Returns child exit code. */
int run_function_capture_output(void(*fn)(void), struct captured_output* out);

/* Runs the shell-tp1-implementation passing 'line' as single input command, capturing output. */
int run_shell_command_capture_output(const char* line, struct captured_output* out);

/* Runs an external command with given argv[] in a child, capturing output. */
int run_command_capture_output(char* const argv[], struct captured_output* out);

/* A test macro that increments the global counters and prints pass/fail with small unicode art. */
#define TEST(name) static bool test_##name(void)

#define RUN_TEST(name) do {                     \
bool result = test_##name();                \
tests_run++;                                \
if(!result){                                \
tests_failed++;                         \
printf("  ❌  %s : " #name "\n", __func__);  \
} else {                                    \
printf("  ✅  %s : " #name "\n", __func__);  \
}                                           \
} while(0)

#endif
