#include "basic-test.h"
#include "test_common.h"

#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include "../src/worker.h"

#include <stdio.h>
#include <math.h>

static int tests_run=0, tests_failed=0;
static char g_test_fail_reason[256];

static int almost_equal(double a, double b, double eps) {
    return (fabs(a - b) < eps);
}

TEST(fifo) {
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
    if (!almost_equal(rep.avg_wait, w, 0.01) ||
        !almost_equal(rep.avg_turnaround, t, 0.01) ||
        !almost_equal(rep.avg_response, r, 0.01) ||
        rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_fifo => mismatch, got W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_FIFO);
    return true;
}

TEST(rr) {
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
    if (!almost_equal(rep.avg_wait, w, 0.01) ||
        !almost_equal(rep.avg_turnaround, t, 0.01) ||
        !almost_equal(rep.avg_response, r, 0.01) ||
        rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_rr => mismatch, got W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_RR);
    return true;
}

TEST(cfs) {
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
    if (!almost_equal(rep.avg_wait, w, 0.01) ||
        !almost_equal(rep.avg_turnaround, t, 0.01) ||
        !almost_equal(rep.avg_response, r, 0.01) ||
        rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_cfs => mismatch, got W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS);
    return true;
}

TEST(bfs) {
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
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_bfs => mismatch => expected procs=3, preempt>0, got procs=%llu, preempts=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_BFS);
    return true;
}

TEST(pipeline) {
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}

TEST(distributed) {
    os_init();
    os_run_distributed_example();
    os_cleanup();
    return true;
}

TEST(fifo_strict) {
    os_init();
    process_t p[2];
    init_process(&p[0], 3, 10, 0);
    init_process(&p[1], 4, 20, 0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p, 2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    double w=1.5, t=5.0, r=1.5;
    if (!almost_equal(rep.avg_wait, w, 0.01) ||
        !almost_equal(rep.avg_turnaround, t, 0.01) ||
        !almost_equal(rep.avg_response, r, 0.01) ||
        rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_fifo_strict => mismatch => W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    return true;
}

void run_basic_tests(int* total, int* passed){
    tests_run=0;
    tests_failed=0;

    printf("\n" CLR_BOLD CLR_YELLOW "╔════════════ BASIC TESTS START ═════════════╗" CLR_RESET "\n");

    RUN_TEST(fifo);
    RUN_TEST(rr);
    RUN_TEST(cfs);
    RUN_TEST(bfs);
    RUN_TEST(pipeline);
    RUN_TEST(distributed);
    RUN_TEST(fifo_strict);

    *total  = tests_run;
    *passed = (tests_run - tests_failed);

    printf(CLR_BOLD CLR_YELLOW "╔══════════════════════════════════════════════╗\n");
    printf("║       BASIC TESTS RESULTS: %d / %d passed      ║\n", *passed, *total);
    if(*passed < *total) {
        printf("║    FAILURES => see above logs for reasons    ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\n" CLR_RESET);
}
