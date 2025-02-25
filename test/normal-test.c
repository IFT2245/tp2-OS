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
static bool check_stats(const sched_report_t* r, double w, double t, double resp,
                        unsigned long long pre, double eps)
{
    if (!almost_equal(r->avg_wait, w, eps)) return false;
    if (!almost_equal(r->avg_turnaround, t, eps)) return false;
    if (!almost_equal(r->avg_response, resp, eps)) return false;
    if (r->preemptions != pre) return false;
    return true;
}

/*
  SJF => 3 procs => p0=1, p1=5, p2=2 => non-preempt =>
   order => p0->p2->p1
   p0: wait=0,  tat=1
   p2: wait=1,  tat=3
   p1: wait=3,  tat=8
   avgWait= (0+1+3)/3=1.33..., TAT= (1+3+8)/3=4.0, resp=1.33..., preempt=0
*/
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

    if (!check_stats(&rep, 1.3333, 4.0, 1.3333, 0ULL, 0.01)) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_sjf => mismatch => got wait=%.2f,tat=%.2f,resp=%.2f,pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_SJF);
    return true;
}

/*
  STRF => partial => quantum=2 => 2 procs => p0=4, p1=3 => we expect at least 1 preempt
*/
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
                 "test_strf => mismatch => expect total=2, preempt>0, got total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_STRF);
    return true;
}

/*
  HRRN => no preempt => 3 procs => p0=2, p1=3, p2=4 => we just check total=3, preempt=0
*/
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
                 "test_hrrn => mismatch => expect total=3, preempt=0, got total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN);
    return true;
}

/* HRRN-RT => partial => 2 procs => p0=3, p1=4 => at least 1 preempt. */
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
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN_RT);
    return true;
}

/*
  PRIORITY => 3 procs => p0=2,prio=3, p1=2,prio=1, p2=2,prio=2
   order => p1->p2->p0 =>
   p1: wait=0,tat=2
   p2: wait=2,tat=4
   p0: wait=4,tat=6
   avgWait=2, tat=4, resp=2 => pre=0
*/
TEST(priority) {
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
    if (!almost_equal(rep.avg_wait, w, 0.01) ||
        !almost_equal(rep.avg_turnaround, t, 0.01) ||
        !almost_equal(rep.avg_response, r, 0.01) ||
        rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_prio => mismatch => got W=%.2f,T=%.2f,R=%.2f,pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_PRIORITY);
    return true;
}

/* CFS-SRTF => partial => 3 procs => p0=2, p1=4, p2=6 => expect preempt>0 */
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
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS_SRTF);
    return true;
}

/*
  SJF strict => 2 procs => p0=2, p1=5 =>
   p0 wait=0, tat=2 => p1 wait=2, tat=7 =>
   avgWait=1, tat=4.5 => resp=1, preempt=0
*/
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

    double w=1.0, t=4.5, r=1.0;
    if (!almost_equal(rep.avg_wait, w, 0.01) ||
        !almost_equal(rep.avg_turnaround, t, 0.01) ||
        !almost_equal(rep.avg_response, r, 0.01) ||
        rep.preemptions != 0ULL) {
        snprintf(g_test_fail_reason, sizeof(g_test_fail_reason),
                 "test_sjf_strict => mismatch => got W=%.2f,T=%.2f,R=%.2f, pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        return false;
    }
    return true;
}

void run_normal_tests(int* total, int* passed) {
    tests_run  = 0;
    tests_failed = 0;

    RUN_TEST(sjf);
    RUN_TEST(strf);
    RUN_TEST(hrrn);
    RUN_TEST(hrrn_rt);
    RUN_TEST(priority);
    RUN_TEST(cfs_srtf);
    RUN_TEST(sjf_strict);

    *total  = tests_run;
    *passed = tests_run - tests_failed;
}
