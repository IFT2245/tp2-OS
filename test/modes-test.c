#include "modes-test.h"
#include "test_common.h"

#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/process.h"
#include "../src/scoreboard.h"

#include <stdio.h>
#include <math.h>

static int tests_run=0, tests_failed=0;
static char g_test_fail_reason[256];

static int almost_equal(double a, double b, double eps) {
    return (fabs(a - b) < eps);
}

/* HPC overshadow test within modes suite. */
TEST(hpc_over) {
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy, 1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs != 0 || rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_hpc_over => HPC overshadow => expected 0 stats, got procs=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HPC_OVERSHADOW);
    return true;
}

/* multiple ephemeral containers in normal usage. */
TEST(multi_containers) {
    os_init();
    for (int i=0; i<2; i++) os_create_ephemeral_container();
    for (int i=0; i<2; i++) os_remove_ephemeral_container();
    os_cleanup();
    return true;
}

/* multiple distributed calls. */
TEST(multi_distrib) {
    os_init();
    os_run_distributed_example();
    os_run_distributed_example();
    os_cleanup();
    return true;
}

/* pipeline test again in modes suite. */
TEST(pipeline_modes) {
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}

/* test mixing multiple algorithms sequentially. */
TEST(mix_algos) {
    os_init();
    process_t p[2];
    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    sched_report_t r1;
    scheduler_fetch_report(&r1);

    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,2);
    sched_report_t r2;
    scheduler_fetch_report(&r2);

    os_cleanup();

    /* Just minimal checks */
    if (r1.total_procs != 2) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_mix_algos => FIFO => mismatch => total=%llu", r1.total_procs);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    if (r2.total_procs != 2) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_mix_algos => BFS => mismatch => total=%llu", r2.total_procs);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    return true;
}

/* double HPC overshadow test. */
TEST(double_hpc) {
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r1;
    scheduler_fetch_report(&r1);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r2;
    scheduler_fetch_report(&r2);

    os_cleanup();

    if (r1.total_procs != 0 || r2.total_procs != 0) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_double_hpc => overshadow => expected 0 procs each, got r1=%llu,r2=%llu",
                 r1.total_procs, r2.total_procs);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    return true;
}

/* MLFQ check test. */
TEST(mlfq_check) {
    os_init();
    process_t p[3];
    init_process(&p[0],2,10,0);
    init_process(&p[1],3,20,0);
    init_process(&p[2],4,30,0);

    scheduler_select_algorithm(ALG_MLFQ);
    scheduler_run(p,3);

    sched_report_t r;
    scheduler_fetch_report(&r);
    os_cleanup();

    if (r.total_procs != 3 || r.preemptions < 1) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_mlfq_check => mismatch => total=%llu, preempt=%llu",
                 r.total_procs, r.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_MLFQ);
    return true;
}

void run_modes_tests(int* total, int* passed) {
    tests_run   = 0;
    tests_failed= 0;

    printf("\n\033[1m\033[93m╔══════════ MODES TESTS START ══════════╗\033[0m\n");

    RUN_TEST(hpc_over);
    RUN_TEST(multi_containers);
    RUN_TEST(multi_distrib);
    RUN_TEST(pipeline_modes);
    RUN_TEST(mix_algos);
    RUN_TEST(double_hpc);
    RUN_TEST(mlfq_check);

    *total  = tests_run;
    *passed = tests_run - tests_failed;

    printf("\033[1m\033[93m╔══════════════════════════════════════════════╗\n");
    printf("║       MODES TESTS RESULTS: %d / %d passed       ║\n", *passed, *total);
    if(*passed < *total) {
        printf("║    FAILURES => see above logs for reasons    ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
}
