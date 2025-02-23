#include "edge-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include <stdio.h>
#include <math.h>

static int tests_run=0, tests_failed=0;
extern char g_test_fail_reason[256];

static int almost_equal(double a, double b, double eps) {
    return (fabs(a - b) < eps);
}

/* single proc => extreme long => p0=50 => wait=0,tat=50,resp=0 => pre=0 */
TEST(extreme_long) {
    os_init();
    process_t p[1];
    init_process(&p[0],50,2,0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs!=1 || rep.preemptions!=0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_extreme_long => mismatch => total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        return false;
    }
    if (!almost_equal(rep.avg_wait, 0.0, 0.001) ||
        !almost_equal(rep.avg_turnaround, 50.0, 0.1) ||
        !almost_equal(rep.avg_response, 0.0, 0.001)) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_extreme_long => stats mismatch => W=%.2f,T=%.2f,R=%.2f",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response);
        return false;
    }
    return true;
}

/* single proc => extreme short => p0=1 => wait=0,tat=1,resp=0 => pre=0 */
TEST(extreme_short) {
    os_init();
    process_t p[1];
    init_process(&p[0],1,2,0);

    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs!=1 || rep.preemptions!=0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_extreme_short => mismatch => total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        return false;
    }
    if (!almost_equal(rep.avg_wait, 0.0, 0.001) ||
        !almost_equal(rep.avg_turnaround, 1.0, 0.01)) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_extreme_short => stats mismatch => W=%.2f,T=%.2f",
                 rep.avg_wait, rep.avg_turnaround);
        return false;
    }
    return true;
}

/* high load => 10 procs => run cfs => check total=10 */
TEST(high_load) {
    os_init();
    process_t arr[10];
    for (int i=0; i<10; i++) {
        init_process(&arr[i], 3+(i%3), 1, 0);
    }
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(arr,10);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs != 10) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_high_load => mismatch => total=%llu, expected=10",
                 rep.total_procs);
        return false;
    }
    return true;
}

/* HPC overshadow under load => pass if no crash */
TEST(hpc_under_load) {
    os_init();
    os_run_hpc_overshadow();
    os_cleanup();
    return true;
}

/* container spam => pass if no crash */
TEST(container_spam) {
    os_init();
    for (int i=0;i<3;i++) {
        os_create_ephemeral_container();
    }
    for (int i=0;i<3;i++) {
        os_remove_ephemeral_container();
    }
    os_cleanup();
    return true;
}

/* pipeline edge => pass if no crash */
TEST(pipeline_edge) {
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}

/* multi distrib => pass if no crash */
TEST(multi_distrib) {
    os_init();
    for (int i=0;i<3;i++) {
        os_run_distributed_example();
    }
    os_cleanup();
    return true;
}

void run_edge_tests(int* total,int* passed) {
    tests_run=0;
    tests_failed=0;

    RUN_TEST(extreme_long);
    RUN_TEST(extreme_short);
    RUN_TEST(high_load);
    RUN_TEST(hpc_under_load);
    RUN_TEST(container_spam);
    RUN_TEST(pipeline_edge);
    RUN_TEST(multi_distrib);

    *total = tests_run;
    *passed= tests_run - tests_failed;
}
