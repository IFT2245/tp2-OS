#include "basic-test.h"

#include "../src/scheduler.h"
#include "../src/process.h"
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
    // if (stats_get_test_fifo()==1) return true;
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
    // stats_set_test_fifo(1);
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
    // stats_set_test_rr(1);
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

/*
   BFS test => disallow zero preemptions.
   We do a "dummy run" to set BFS quantum, then run a big process => must see preemptions>=1.
*/
static bool test_bfs(void) {
    g_tests_run++;
    os_init();

    /* 1) Dummy BFS run with small process => BFS sets quantum. */
    process_t dummy[1];
    init_process(&dummy[0], 1, 1, 0); /* burst=1 */
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(dummy, 1);

    unsigned long q = scheduler_get_bfs_quantum();

    /* 2) Real BFS test => one big process => burst=(q+2). BFS must do at least one preemption. */
    process_t bigP[1];
    init_process(&bigP[0], q+2, 1, 0); /* bigger than BFS quantum => must preempt */

    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(bigP, 1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.preemptions < 1) {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
                 "test_bfs => zero preemption is NOT fine => BFS quantum=%lu => single burst=%lu => preempts=%llu => FAIL",
                 q, (unsigned long)(q+2), (unsigned long long)rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }

    /* If we get here => BFS had at least 1 preemption => pass. */
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
        printf(CLR_RED CLR_BOLD"  PASS: %s\n"CLR_RESET, basic_tests[i].name);
        return;
    } else
    {
        *pass_out=1;
        printf(CLR_RED CLR_BOLD"  FAIL: %s => %s\n"CLR_RESET,
               basic_tests[i].name,
               test_get_fail_reason());
        // stats_set_specific_test_reason(test_get_fail_reason(), i); // TO DO TO EVERY TESTS FOR STATS
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
            printf(CLR_RED CLR_BOLD"  PASS: %s\n"CLR_RESET, basic_tests[i].name);
        } else {
            printf(CLR_RED CLR_BOLD"  FAIL: %s => %s\n"CLR_RESET,
                   basic_tests[i].name,
                   test_get_fail_reason());
            // stats_set_current_test_state(basic_tests[i].name, ok);  // TO DO TO EVERY TESTS FOR STATS with dynamic
            // stats_set_fifo_reason(test_get_fail_reason()); // TO DO TO EVERY TESTS FOR STATS with dynamic
        }

        if (skip_remaining_tests_requested()) { // to do for every levels
            *total  = g_tests_run;
            *passed = g_tests_run - g_tests_failed;
            stats_inc_tests_passed(*passed);
            stats_inc_tests_failed((*total) - (*passed));
            printf(CLR_RED "[SIGTERM] => skipping remaining tests in this suite.\n" CLR_RESET);
            break;
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
