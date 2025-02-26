#include "normal-test.h"
#include "test_common.h"
#include "../src/runner.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include "../src/stats.h"
#include <stdio.h>
#include <math.h>

static int g_tests_run=0, g_tests_failed=0;
static char g_fail_reason[256];

static int almost_equal(double a, double b, double eps) {
    return (fabs(a-b) < eps);
}

/* Test functions */
static bool test_sjf(void) {
    g_tests_run++;
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

    if(!(almost_equal(rep.avg_wait, 1.33, 0.5) &&
         almost_equal(rep.avg_turnaround, 4.0, 0.5) &&
         almost_equal(rep.avg_response, 1.33, 0.5) &&
         rep.preemptions==0ULL))
    {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
            "test_sjf => mismatch => W=%.2f,T=%.2f,R=%.2f, pre=%llu",
            rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_SJF);
    return true;
}

static bool test_strf(void) {
    g_tests_run++;
    os_init();
    process_t p[2];
    init_process(&p[0],4,1,0);
    init_process(&p[1],3,1,0);

    scheduler_select_algorithm(ALG_STRF);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=2 || rep.preemptions<1) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_strf => mismatch => procs=%llu, preempts=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_STRF);
    return true;
}

static bool test_hrrn(void) {
    g_tests_run++;
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

    if(rep.total_procs!=3 || rep.preemptions!=0ULL) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_hrrn => mismatch => total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN);
    return true;
}

static bool test_hrrn_rt(void) {
    g_tests_run++;
    os_init();
    process_t p[2];
    init_process(&p[0],3,1,0);
    init_process(&p[1],4,1,0);

    scheduler_select_algorithm(ALG_HRRN_RT);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=2 || rep.preemptions<1) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_hrrn_rt => mismatch => total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN_RT);
    return true;
}

static bool test_priority(void) {
    g_tests_run++;
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

    if(rep.preemptions!=0ULL ||
       !almost_equal(rep.avg_wait, 2.0, 1.0) ||
       !almost_equal(rep.avg_turnaround,4.0,1.0))
    {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_priority => mismatch => W=%.2f,T=%.2f,R=%.2f,pre=%llu",
                 rep.avg_wait, rep.avg_turnaround, rep.avg_response, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_PRIORITY);
    return true;
}

static bool test_cfs_srtf(void) {
    g_tests_run++;
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

    if(rep.total_procs!=3 || rep.preemptions<1) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_cfs_srtf => mismatch => total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS_SRTF);
    return true;
}

static bool test_sjf_strict(void) {
    g_tests_run++;
    os_init();
    process_t p[2];
    init_process(&p[0],2,10,0);
    init_process(&p[1],5,20,0);

    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.preemptions!=0ULL) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_sjf_strict => mismatch => preempt=%llu",
                 rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

/* Array of all normal tests */
typedef bool (*test_fn)(void);
static struct {
    const char* name;
    test_fn func;
} normal_tests[] = {
    {"sjf",       test_sjf},
    {"strf",      test_strf},
    {"hrrn",      test_hrrn},
    {"hrrn_rt",   test_hrrn_rt},
    {"priority",  test_priority},
    {"cfs_srtf",  test_cfs_srtf},
    {"sjf_strict",test_sjf_strict}
};
static const int NORMAL_COUNT = sizeof(normal_tests)/sizeof(normal_tests[0]);

int normal_test_count(void) {
    return NORMAL_COUNT;
}
const char* normal_test_name(int i) {
    if(i<0 || i>=NORMAL_COUNT) return NULL;
    return normal_tests[i].name;
}

void normal_test_run_single(int i, int* pass_out) {
    if(!pass_out) return;
    if(i<0 || i>=NORMAL_COUNT) {
        *pass_out=0;
        return;
    }
    g_tests_run=0;
    g_tests_failed=0;
    memset(g_fail_reason,0,sizeof(g_fail_reason));

    bool ok = normal_tests[i].func();
    *pass_out = (ok && g_tests_failed==0) ? 1 : 0;
}

void run_normal_tests(int* total,int* passed) {
    g_tests_run=0;
    g_tests_failed=0;
    memset(g_fail_reason,0,sizeof(g_fail_reason));

    printf("\n\033[1m\033[93m╔══════════ NORMAL TESTS START ══════════╗\033[0m\n");
    for(int i=0; i<NORMAL_COUNT; i++) {
        if (skip_remaining_tests_requested()) {
            printf(CLR_RED "[SIGTERM] => skipping remaining tests in this suite.\n" CLR_RESET);
            break;
        }

        bool ok = normal_tests[i].func();
        if(ok){
            printf("  PASS: %s\n", normal_tests[i].name);
        } else {
            printf("  FAIL: %s => %s\n", normal_tests[i].name, test_get_fail_reason());
        }
    }

    *total  = g_tests_run;
    *passed = (g_tests_run - g_tests_failed);

    scoreboard_update_normal(*total, *passed);
    stats_inc_tests_passed(*passed);
    stats_inc_tests_failed((*total) - (*passed));

    printf("\033[1m\033[93m╔══════════════════════════════════════════════╗\n");
    printf("║      NORMAL TESTS RESULTS: %d / %d passed       ║\n", *passed, *total);
    if(*passed < *total) {
        printf("║    FAILURES => see logs above               ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
}
