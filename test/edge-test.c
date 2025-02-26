#include "edge-test.h"
#include "test_common.h"
#include "../src/stats.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include "../src/runner.h"
#include <stdio.h>
#include <math.h>

static int g_tests_run=0, g_tests_failed=0;
static char g_fail_reason[256];

static int almost_equal(double a, double b, double eps) {
    return (fabs(a - b) < eps);
}

static bool test_extreme_long(void) {
    g_tests_run++;
    os_init();
    process_t p[1];
    init_process(&p[0],50,2,0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=1 || rep.preemptions!=0ULL) {
        snprintf(g_fail_reason, sizeof(g_fail_reason),
                 "test_extreme_long => mismatch => total=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    if(!almost_equal(rep.avg_wait,0.0,0.9) ||
       !almost_equal(rep.avg_turnaround,50.0,5.0))
    {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_extreme_long => stats mismatch => W=%.2f,T=%.2f",
                 rep.avg_wait, rep.avg_turnaround);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

static bool test_extreme_short(void) {
    g_tests_run++;
    os_init();
    process_t p[1];
    init_process(&p[0],1,2,0);

    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=1 || rep.preemptions!=0ULL) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_extreme_short => mismatch => procs=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    if(!almost_equal(rep.avg_wait, 0.0, 0.5) ||
       !almost_equal(rep.avg_turnaround,1.0,0.5))
    {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_extreme_short => mismatch => W=%.2f,T=%.2f",
                 rep.avg_wait, rep.avg_turnaround);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

static bool test_high_load(void) {
    g_tests_run++;
    os_init();
    process_t arr[10];
    for(int i=0;i<10;i++){
        init_process(&arr[i],3+(i%3), 1, 0);
    }
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(arr,10);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs != 10) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_high_load => mismatch => total=%llu",
                 rep.total_procs);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

static bool test_hpc_under_load(void) {
    g_tests_run++;
    os_init();
    os_run_hpc_overshadow();
    os_cleanup();
    return true;
}

static bool test_container_spam(void) {
    g_tests_run++;
    os_init();
    for (int i=0;i<3;i++){
        os_create_ephemeral_container();
    }
    for (int i=0;i<3;i++){
        os_remove_ephemeral_container();
    }
    os_cleanup();
    return true;
}

static bool test_pipeline_edge(void) {
    g_tests_run++;
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}

static bool test_multi_distrib(void) {
    g_tests_run++;
    os_init();
    for(int i=0;i<3;i++){
        os_run_distributed_example();
    }
    os_cleanup();
    return true;
}

typedef bool (*test_fn)(void);
static struct {
    const char* name;
    test_fn func;
} edge_tests[] = {
    {"extreme_long",   test_extreme_long},
    {"extreme_short",  test_extreme_short},
    {"high_load",      test_high_load},
    {"hpc_under_load", test_hpc_under_load},
    {"container_spam", test_container_spam},
    {"pipeline_edge",  test_pipeline_edge},
    {"multi_distrib",  test_multi_distrib}
};
static const int EDGE_COUNT = sizeof(edge_tests)/sizeof(edge_tests[0]);

int edge_test_count(void){ return EDGE_COUNT; }
const char* edge_test_name(int i){
    if(i<0||i>=EDGE_COUNT) return NULL;
    return edge_tests[i].name;
}

void edge_test_run_single(int i,int* pass_out){
    if(!pass_out) return;
    if(i<0||i>=EDGE_COUNT){
        *pass_out=0;
        return;
    }
    g_tests_run=0;
    g_tests_failed=0;
    bool ok = edge_tests[i].func();
    *pass_out = (ok && g_tests_failed==0) ? 1 : 0;
}

void run_edge_tests(int* total,int* passed){
    g_tests_run=0;
    g_tests_failed=0;

    printf("\n\033[1m\033[93m╔════════════ EDGE TESTS START ═══════════╗\033[0m\n");
    for(int i=0;i<EDGE_COUNT;i++){
        if (skip_remaining_tests_requested()) {
            printf(CLR_RED "[SIGTERM] => skipping remaining tests in this suite.\n" CLR_RESET);
            break;
        }

        bool ok = edge_tests[i].func();
        if(ok){
            printf("  PASS: %s\n", edge_tests[i].name);
        } else {
            printf("  FAIL: %s => %s\n", edge_tests[i].name, test_get_fail_reason());
        }
    }

    *total = g_tests_run;
    *passed= (g_tests_run - g_tests_failed);

    scoreboard_update_edge(*total, *passed);
    stats_inc_tests_passed(*passed);
    stats_inc_tests_failed((*total)-(*passed));

    printf("\033[1m\033[93m╔══════════════════════════════════════════════╗\n");
    printf("║      EDGE TESTS RESULTS: %d / %d passed        ║\n", *passed, *total);
    if(*passed < *total){
        printf("║    FAILURES => see logs above               ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
}
