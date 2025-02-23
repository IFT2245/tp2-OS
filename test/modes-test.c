#include "modes-test.h"
#include "test_common.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/process.h"
#include "../src/scoreboard.h"
#include <stdio.h>
#include <math.h>

static int tests_run=0, tests_failed=0;
extern char g_test_fail_reason[256];

/* HPC overshadow => we expect 0 normal stats */
TEST(hpc_over) {
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy, 1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs != 0 || rep.preemptions != 0ULL ||
        rep.avg_wait != 0.0 || rep.avg_turnaround != 0.0 || rep.avg_response != 0.0) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_hpc_over => HPC overshadow => expected 0 stats, got procs=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HPC_OVERSHADOW);
    return true;
}

/* multi containers => pass if no crash */
TEST(multi_containers) {
    os_init();
    for (int i=0; i<2; i++) os_create_ephemeral_container();
    for (int i=0; i<2; i++) os_remove_ephemeral_container();
    os_cleanup();
    return true;
}

/* multi distrib => pass if no crash */
TEST(multi_distrib) {
    os_init();
    os_run_distributed_example();
    os_run_distributed_example();
    os_cleanup();
    return true;
}

/* pipeline modes => pass if no crash */
TEST(pipeline_modes) {
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}

/* mix => run FIFO then BFS => partial checks */
TEST(mix_algos) {
    os_init();
    process_t p[2];
    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if (r1.total_procs != 2 || r1.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_mix_algos => FIFO part => mismatch => total=%llu, preempt=%llu",
                 r1.total_procs, r1.preemptions);
        os_cleanup();
        return false;
    }

    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,2);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();

    if (r2.total_procs != 2 || r2.preemptions < 1) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_mix_algos => BFS part => mismatch => total=%llu, preempt=%llu",
                 r2.total_procs, r2.preemptions);
        return false;
    }
    return true;
}

/* double HPC => each run => 0 stats. */
TEST(double_hpc) {
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if (r1.total_procs != 0) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_double_hpc => overshadow #1 => expected 0 total_procs, got %llu",
                 r1.total_procs);
        os_cleanup();
        return false;
    }

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();

    if (r2.total_procs != 0) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_double_hpc => overshadow #2 => expected 0 total_procs, got %llu",
                 r2.total_procs);
        return false;
    }
    return true;
}

/* MLFQ => 3 procs => partial => expect preempt>0 */
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
        return false;
    }
    scoreboard_set_sc_mastered(ALG_MLFQ);
    return true;
}

void run_modes_tests(int* total, int* passed) {
    tests_run   = 0;
    tests_failed= 0;

    RUN_TEST(hpc_over);
    RUN_TEST(multi_containers);
    RUN_TEST(multi_distrib);
    RUN_TEST(pipeline_modes);
    RUN_TEST(mix_algos);
    RUN_TEST(double_hpc);
    RUN_TEST(mlfq_check);

    *total  = tests_run;
    *passed = tests_run - tests_failed;
}
