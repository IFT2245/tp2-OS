#include "normal-test.h"
#include "test_common.h"

#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include "../src/scoreboard.h"

#include <stdio.h>
#include <math.h>

static int tests_run=0, tests_failed=0;
static char g_test_fail_reason[256];

static int almost_equal(double a, double b, double eps) {
    return (fabs(a-b) < eps);
}

static bool check_stats(const sched_report_t* r,
                        double w, double t, double resp,
                        unsigned long long pre,
                        double eps)
{
    if (!almost_equal(r->avg_wait, w, eps)) return false;
    if (!almost_equal(r->avg_turnaround, t, eps)) return false;
    if (!almost_equal(r->avg_response, resp, eps)) return false;
    if (r->preemptions != pre) return false;
    return true;
}

/* SJF test. */
TEST(sjf) {
    os_init();
    process_t p[3];
    init_process(&p[0],1,1,0);
    init_process(&p[1],5,1,0);
    init_process(&p[2],2,1,0);

    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    /* sum waits => ~4 => avg=1.33..., sum TAT => ~12 => avg=4.0 */
    if (!check_stats(&rep, 1.33, 4.0, 1.33, 0ULL, 0.5)) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_sjf => mismatch => W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_SJF);
    return true;
}

/* STRF test. */
TEST(strf) {
    os_init();
    process_t p[2];
    init_process(&p[0],4,1,0);
    init_process(&p[1],3,1,0);

    scheduler_select_algorithm(ALG_STRF);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs != 2 || rep.preemptions < 1) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_strf => mismatch => procs=%llu, preempts=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_STRF);
    return true;
}

/* HRRN test. */
TEST(hrrn) {
    os_init();
    process_t p[3];
    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);
    init_process(&p[2],4,1,0);

    scheduler_select_algorithm(ALG_HRRN);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs != 3 || rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_hrrn => mismatch => total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN);
    return true;
}

/* HRRN-RT test. */
TEST(hrrn_rt) {
    os_init();
    process_t p[2];
    init_process(&p[0],3,1,0);
    init_process(&p[1],4,1,0);

    scheduler_select_algorithm(ALG_HRRN_RT);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs!=2 || rep.preemptions<1) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_hrrn_rt => mismatch => total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN_RT);
    return true;
}

/* Priority test. */
TEST(priority_test) {
    os_init();
    process_t p[3];
    init_process(&p[0],2,3,0);
    init_process(&p[1],2,1,0);
    init_process(&p[2],2,2,0);

    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    double w=2.0, t=4.0, r=2.0;
    if (!almost_equal(rep.avg_wait, w, 1.0) ||
        !almost_equal(rep.avg_turnaround, t, 1.0) ||
        !almost_equal(rep.avg_response, r, 1.0) ||
        rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_priority => mismatch => W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_PRIORITY);
    return true;
}

/* CFS-SRTF test. */
TEST(cfs_srtf) {
    os_init();
    process_t p[3];
    init_process(&p[0],2,1,0);
    init_process(&p[1],4,1,0);
    init_process(&p[2],6,1,0);

    scheduler_select_algorithm(ALG_CFS_SRTF);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if (rep.total_procs!=3 || rep.preemptions<1) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_cfs_srtf => mismatch => total=%llu, preempts=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS_SRTF);
    return true;
}

/* Another SJF strict variant. */
TEST(sjf_strict) {
    os_init();
    process_t p[2];
    init_process(&p[0],2,10,0);
    init_process(&p[1],5,20,0);

    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    /* We only check preemptions=0 for SJF (non-preemptive). */
    if (rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_sjf_strict => mismatch => preempt=%llu",
                 rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    return true;
}

void run_normal_tests(int* total, int* passed) {
    tests_run  = 0;
    tests_failed = 0;

    printf("\n\033[1m\033[93m╔══════════ NORMAL TESTS START ══════════╗\033[0m\n");

    RUN_TEST(sjf);
    RUN_TEST(strf);
    RUN_TEST(hrrn);
    RUN_TEST(hrrn_rt);
    RUN_TEST(priority_test);
    RUN_TEST(cfs_srtf);
    RUN_TEST(sjf_strict);

    *total  = tests_run;
    *passed = tests_run - tests_failed;

    printf("\033[1m\033[93m╔══════════════════════════════════════════════╗\n");
    printf("║      NORMAL TESTS RESULTS: %d / %d passed       ║\n", *passed, *total);
    if(*passed < *total) {
        printf("║    FAILURES => see above logs for reasons    ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
}
