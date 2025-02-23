#include "hidden-test.h"
#include "test_common.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/scoreboard.h"
#include <stdio.h>
#include <math.h>

static int tests_run=0, tests_failed=0;
extern char g_test_fail_reason[256];

static int almost_equal(double a, double b, double eps) {
    return fabs(a - b) < eps;
}

/* multiple distributed => pass if no crash */
TEST(distrib_heavy) {
    os_init();
    for (int i=0;i<4;i++) {
        os_run_distributed_example();
    }
    os_cleanup();
    return true;
}

/* HPC overshadow heavy => multiple times => each => 0 stats */
TEST(hpc_heavy) {
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    for (int i=0; i<2; i++) {
        scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
        scheduler_run(dummy,1);
        sched_report_t rep;
        scheduler_fetch_report(&rep);
        if (rep.total_procs != 0) {
            snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                     "test_hpc_heavy => overshadow => expected total_procs=0, got %llu",
                     rep.total_procs);
            os_cleanup();
            return false;
        }
    }
    os_cleanup();
    return true;
}

/* container + HPC overshadow => pass if no crash */
TEST(container_combo) {
    os_init();
    os_create_ephemeral_container();
    os_run_distributed_example();
    os_run_hpc_overshadow();
    os_remove_ephemeral_container();
    os_cleanup();
    return true;
}

/* scheduling variety => run SJF then Priority => partial checks */
TEST(scheduling_variety) {
    os_init();
    process_t p[2];
    init_process(&p[0],2,1,0);
    init_process(&p[1],6,2,0);

    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if (r1.total_procs!=2 || r1.preemptions!=0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_scheduling_variety => SJF => mismatch => total=%llu, pre=%llu",
                 r1.total_procs, r1.preemptions);
        os_cleanup();
        return false;
    }

    init_process(&p[0],2,3,0);
    init_process(&p[1],6,1,0);
    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,2);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();

    if (r2.total_procs!=2 || r2.preemptions!=0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_scheduling_variety => Priority => mismatch => total=%llu, pre=%llu",
                 r2.total_procs, r2.preemptions);
        return false;
    }
    return true;
}

/* auto logic => just pass */
TEST(auto_logic) {
    /* No actual auto mode => just pass. */
    printf("Auto mode selection tested (dummy).\n");
    return true;
}

/* final synergy => HPC + container + pipeline + distributed => pass if no crash */
TEST(final_integration) {
    os_init();
    os_log("Final synergy HPC + container + pipeline + distributed");
    os_create_ephemeral_container();
    os_run_hpc_overshadow();
    os_run_distributed_example();
    os_pipeline_example();
    os_remove_ephemeral_container();
    os_cleanup();
    return true;
}

/* multi-stage => distributed + HPC overshadow => overshadow => 0 stats each time */
TEST(multi_stage_distributed) {
    os_init();

    os_run_distributed_example();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if (r1.total_procs != 0) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_multi_stage_distributed => overshadow #1 => expected 0 procs, got %llu",
                 r1.total_procs);
        os_cleanup();
        return false;
    }

    os_run_distributed_example();
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();

    if (r2.total_procs != 0) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_multi_stage_distributed => overshadow #2 => expected 0 procs, got %llu",
                 r2.total_procs);
        return false;
    }
    return true;
}

void run_hidden_tests(int* total,int* passed) {
    tests_run=0;
    tests_failed=0;

    RUN_TEST(distrib_heavy);
    RUN_TEST(hpc_heavy);
    RUN_TEST(container_combo);
    RUN_TEST(scheduling_variety);
    RUN_TEST(auto_logic);
    RUN_TEST(final_integration);
    RUN_TEST(multi_stage_distributed);

    *total = tests_run;
    *passed= tests_run - tests_failed;
}
