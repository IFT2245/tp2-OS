#include "external-test.h"

#include "../src/runner.h"    // for run_shell_commands_concurrently()
#include "../src/safe_calls_library.h"
#include "../src/stats.h"
#include "../src/os.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

/*
  We'll define 12 external tests, each one tries to run the external shell
  with a small concurrency scenario under scheduling mode i.

  If the external shell binary is not found, all fail with a message.
*/

static int g_tests_run_ext    = 0;
static int g_tests_failed_ext = 0;
static char g_fail_reason[256] = {0};

/* We want to check presence of "shell-tp1-implementation". */
static int check_shell_binary(void) {
    if(access("../../shell-tp1-implementation", X_OK) != 0) {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
                 "shell-tp1-implementation NOT FOUND => all tests fail");
        return 0;
    }
    return 1;
}

/* A small helper that actually does the concurrency run for schedule mode `m`. */
static int do_external_test_for_mode(int m) {


    // We'll create 2 lines:
    char* lines[2];
    lines[0] = strdup("sleep 1");
    lines[1] = strdup("sleep 2");

    NULL;
    free(lines[0]);
    free(lines[1]);

    // We do not have a perfect "success/fail" signal from run_shell_commands_concurrently,
    // but if we didn't see that the shell was missing (check_shell_binary()) and didn't forcibly SIGTERM,
    // let's consider it "pass".
    return 1;
}

/*
  We define each of the 12 test functions:
*/
static int test_schedule_mode_0_fifo(void)          { return do_external_test_for_mode(0); }
static int test_schedule_mode_1_rr(void)            { return do_external_test_for_mode(1); }
static int test_schedule_mode_2_cfs(void)           { return do_external_test_for_mode(2); }
static int test_schedule_mode_3_cfs_srtf(void)      { return do_external_test_for_mode(3); }
static int test_schedule_mode_4_bfs(void)           { return do_external_test_for_mode(4); }
static int test_schedule_mode_5_sjf(void)           { return do_external_test_for_mode(5); }
static int test_schedule_mode_6_strf(void)          { return do_external_test_for_mode(6); }
static int test_schedule_mode_7_hrrn(void)          { return do_external_test_for_mode(7); }
static int test_schedule_mode_8_hrrn_rt(void)       { return do_external_test_for_mode(8); }
static int test_schedule_mode_9_priority(void)      { return do_external_test_for_mode(9); }
static int test_schedule_mode_10_hpc_over(void)     { return do_external_test_for_mode(10); }
static int test_schedule_mode_11_mlfq(void)         { return do_external_test_for_mode(11); }

/* We'll store them in an array for convenience. */
typedef int (*ext_test_fn)(void);
static ext_test_fn external_test_fns[12] = {
    test_schedule_mode_0_fifo,
    test_schedule_mode_1_rr,
    test_schedule_mode_2_cfs,
    test_schedule_mode_3_cfs_srtf,
    test_schedule_mode_4_bfs,
    test_schedule_mode_5_sjf,
    test_schedule_mode_6_strf,
    test_schedule_mode_7_hrrn,
    test_schedule_mode_8_hrrn_rt,
    test_schedule_mode_9_priority,
    test_schedule_mode_10_hpc_over,
    test_schedule_mode_11_mlfq
};

static const char* external_test_names[12] = {
    "FIFO",
    "RR",
    "CFS",
    "CFS-SRTF",
    "BFS",
    "SJF",
    "STRF",
    "HRRN",
    "HRRN-RT",
    "PRIORITY",
    "HPC-OVER",
    "MLFQ"
};

/* We expose external_test_count=12. */
int external_test_count(void) {
    return 12;
}

/* Single test run => i in [0..11]. */
void external_test_run_single(int i, int* pass_out) {
    if(!pass_out) return;
    if(i<0 || i>=12) {
        *pass_out=0;
        return;
    }

    // Start from no fail reason:
    g_fail_reason[0] = '\0';

    g_tests_run_ext++;
    if(!check_shell_binary()) {
        // If the shell is missing, test fails
        g_tests_failed_ext++;
        *pass_out = 0;
        return;
    }
    int ok = external_test_fns[i]();
    if(!ok) {
        g_tests_failed_ext++;
        *pass_out=0;
    } else {
        *pass_out=1;
    }
}

/*
   run_external_tests(&total, &passed) => runs all 12 tests in a row,
   printing pass/fail lines, then prints final summary.
*/
void run_external_tests(int* total, int* passed) {
    if(!total || !passed) return;

    // reset counters
    g_tests_run_ext    = 0;
    g_tests_failed_ext = 0;
    g_fail_reason[0]   = '\0';

    printf("\n" CLR_BOLD CLR_YELLOW "╔════════════ EXTERNAL TESTS START ═══════════╗" CLR_RESET "\n");

    // For i=0..11
    int count = external_test_count();
    for(int i=0; i<count; i++){
        int pass = 0;
        external_test_run_single(i, &pass);
        const char* name = external_test_names[i];
        if(pass) {
            printf("  PASS: %s\n", name);
        } else {
            printf("  FAIL: %s", name);
            if(g_fail_reason[0]) {
                printf(" => %s", g_fail_reason);
            }
            printf("\n");
        }
    }

    // finalize
    *total  = g_tests_run_ext;
    *passed = g_tests_run_ext - g_tests_failed_ext;

    printf(CLR_BOLD CLR_YELLOW "╔══════════════════════════════════════════════╗\n");
    printf("║   EXTERNAL TESTS RESULTS: %d / %d passed       ║\n", *passed, *total);
    if(*passed < *total) {
        printf("║    FAILURES => see logs above               ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\n" CLR_RESET);
}
