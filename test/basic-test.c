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

static int almost_equal(double a, double b, double eps) {
    return (fabs(a - b) < eps);
}
extern char g_test_fail_reason[256];

/*
  test_fifo => 2 procs => p0=3ms, p1=5ms => arrival=0 => no preemption
  p0: wait=0, tat=3
  p1: wait=3, tat=8
  avgWait=1.5, avgTAT=5.5, avgResp=1.5 => preempt=0
*/
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
        return false;
    }
    scoreboard_set_sc_mastered(ALG_FIFO);
    return true;
}

/* test_rr => p0=2, p1=2 => quantum=2 => each finishes in its single slice => no preempt =>
   p0 wait=0, tat=2 => p1 wait=2, tat=4 => avg wait=1, tat=3, resp=1 => preempt=0
*/
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
        return false;
    }
    scoreboard_set_sc_mastered(ALG_RR);
    return true;
}

/* CFS => 2 procs => p0=3, p1=4 => no preempt =>
   p0: wait=0,tat=3 => p1: wait=3,tat=7 =>
   avg wait=1.5, tat=5.0, resp=1.5 => preempt=0
*/
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
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS);
    return true;
}

/* BFS => 3 procs => partial => quantum=2 => expect at least 1 preempt */
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
        return false;
    }
    scoreboard_set_sc_mastered(ALG_BFS);
    return true;
}

/* pipeline => pass if no crash */
TEST(pipeline) {
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}

/* distributed => pass if no crash */
TEST(distributed) {
    os_init();
    os_run_distributed_example();
    os_cleanup();
    return true;
}

/* FIFO strict => 2 procs => p0=3, p1=4 => same as cfs test => W=1.5,T=5.0,R=1.5, pre=0 */
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
        return false;
    }
    return true;
}

void run_basic_tests(int* total, int* passed) {
    tests_run = 0;
    tests_failed = 0;

    RUN_TEST(fifo);
    RUN_TEST(rr);
    RUN_TEST(cfs);
    RUN_TEST(bfs);
    RUN_TEST(pipeline);
    RUN_TEST(distributed);
    RUN_TEST(fifo_strict);

    *total  = tests_run;
    *passed = tests_run - tests_failed;
}
