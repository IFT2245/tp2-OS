#include "basic-test.h"
#include "test_common.h"

#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include "../src/worker.h"

#include <stdio.h>
#include <math.h>

/* Global counters for run_basic_tests() usage: */
static int g_tests_run=0, g_tests_failed=0;
static char g_fail_reason[256];

/* Helper: approximate float eq */
static int almost_equal(double a, double b, double eps) {
    return (fabs(a - b) < eps);
}

/* ---------- Actual test functions ---------- */
static bool test_fifo(void) {
    g_tests_run++;
    os_init();
    process_t p[2];
    init_process(&p[0], 3, 1, 0);
    init_process(&p[1], 5, 1, 0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p, 2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    double w=1.5, t=5.5, r=1.5;
    if (!almost_equal(rep.avg_wait, w, 0.1) ||
        !almost_equal(rep.avg_turnaround, t, 0.1) ||
        !almost_equal(rep.avg_response, r, 0.1) ||
        rep.preemptions != 0ULL)
    {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
                 "test_fifo => mismatch, got W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_FIFO);
    return true;
}

static bool test_rr(void) {
    g_tests_run++;
    os_init();
    process_t p[2];
    init_process(&p[0], 2, 1, 0);
    init_process(&p[1], 2, 1, 0);

    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p, 2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    double w=1.0, t=3.0, r=1.0;
    if (!almost_equal(rep.avg_wait, w, 0.2) ||
        !almost_equal(rep.avg_turnaround, t, 0.2) ||
        !almost_equal(rep.avg_response, r, 0.2) ||
        rep.preemptions != 0ULL)
    {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
                 "test_rr => mismatch, got W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_RR);
    return true;
}

static bool test_cfs(void) {
    g_tests_run++;
    os_init();
    process_t p[2];
    init_process(&p[0], 3, 0, 0);
    init_process(&p[1], 4, 0, 0);

    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(p, 2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    double w=1.5, t=5.0, r=1.5;
    if (!almost_equal(rep.avg_wait, w, 0.2) ||
        !almost_equal(rep.avg_turnaround, t, 0.3) ||
        !almost_equal(rep.avg_response, r, 0.2) ||
        rep.preemptions != 0ULL)
    {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
                 "test_cfs => mismatch => W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS);
    return true;
}

static bool test_bfs(void) {
    g_tests_run++;
    os_init();
    process_t p[3];
    init_process(&p[0], 2, 1, 0);
    init_process(&p[1], 3, 1, 0);
    init_process(&p[2], 4, 1, 0);

    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p, 3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs != 3 || rep.preemptions < 1) {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
                 "test_bfs => mismatch => procs=%llu, preempts=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_BFS);
    return true;
}

static bool test_pipeline(void) {
    g_tests_run++;
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}

static bool test_distributed(void) {
    g_tests_run++;
    os_init();
    os_run_distributed_example();
    os_cleanup();
    return true;
}

static bool test_fifo_strict(void) {
    g_tests_run++;
    os_init();
    process_t p[2];
    init_process(&p[0], 3, 10, 0);
    init_process(&p[1], 4, 20, 0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p, 2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.preemptions != 0ULL) {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
                 "test_fifo_strict => mismatch => preempt=%llu",
                 rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

/* Array of all basic tests so we can run them individually or in bulk */
typedef bool (*test_fn)(void);
static struct {
    const char* name;
    test_fn func;
} basic_tests[] = {
    { "fifo",           test_fifo },
    { "rr",             test_rr },
    { "cfs",            test_cfs },
    { "bfs",            test_bfs },
    { "pipeline",       test_pipeline },
    { "distributed",    test_distributed },
    { "fifo_strict",    test_fifo_strict },
};
static const int BASIC_COUNT = sizeof(basic_tests)/sizeof(basic_tests[0]);

/* Public API: get number of tests in Basic suite */
int basic_test_count(void) {
    return BASIC_COUNT;
}

/* Public API: get name of ith test in Basic suite */
const char* basic_test_name(int i) {
    if(i<0 || i>=BASIC_COUNT) return NULL;
    return basic_tests[i].name;
}

/* Public API: run the ith single test in Basic suite.
   pass_out=1 if test passed, else 0.
*/
void basic_test_run_single(int i, int* pass_out) {
    if(!pass_out) return;
    if(i<0 || i>=BASIC_COUNT) {
        *pass_out=0;
        return;
    }
    /* reset counters for single test run */
    g_tests_run=0;
    g_tests_failed=0;
    memset(g_fail_reason,0,sizeof(g_fail_reason));

    /* run it */
    bool ok = basic_tests[i].func();
    if(!ok) {
        *pass_out=0;
        return;
    }
    /* if not failed => pass */
    *pass_out = (g_tests_failed==0) ? 1 : 0;
}

/* Public API: run all Basic tests in a row */
void run_basic_tests(int* total, int* passed){
    g_tests_run=0;
    g_tests_failed=0;
    memset(g_fail_reason,0,sizeof(g_fail_reason));

    printf("\n" CLR_BOLD CLR_YELLOW "╔════════════ BASIC TESTS START ═════════════╗" CLR_RESET "\n");
    for(int i=0; i<BASIC_COUNT; i++){
        bool ok = basic_tests[i].func();
        if(ok) {
            printf("  PASS: %s\n", basic_tests[i].name);
        } else {
            printf("  FAIL: %s => %s\n",
                   basic_tests[i].name,
                   test_get_fail_reason());
        }
    }

    *total  = g_tests_run;
    *passed = g_tests_run - g_tests_failed;

    printf(CLR_BOLD CLR_YELLOW "╔══════════════════════════════════════════════╗\n");
    printf("║       BASIC TESTS RESULTS: %d / %d passed      ║\n", *passed, *total);
    if(*passed < *total) {
        printf("║    FAILURES => see logs above               ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\n" CLR_RESET);
}
