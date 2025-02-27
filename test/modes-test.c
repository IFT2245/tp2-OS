#include "modes-test.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/process.h"
#include "../src/scoreboard.h"
#include "../src/stats.h"
#include <stdio.h>
#include <math.h>

static int g_tests_run=0, g_tests_failed=0;
static char g_fail_reason[256];

static int almost_equal(double a, double b, double eps) {
    return (fabs(a - b) < eps);
}

/* HPC overshadow */
static bool test_hpc_over(void) {
    g_tests_run++;
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy, 1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=0 || rep.preemptions!=0ULL) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_hpc_over => mismatch => procs=%llu, pre=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HPC_OVERSHADOW);
    return true;
}

static bool test_multi_containers(void) {
    g_tests_run++;
    os_init();
    for(int i=0;i<2;i++){
        os_create_ephemeral_container();
    }
    for(int i=0;i<2;i++){
        os_remove_ephemeral_container();
    }
    os_cleanup();
    return true;
}

static bool test_multi_distrib(void) {
    g_tests_run++;
    os_init();
    os_run_distributed_example();
    os_run_distributed_example();
    os_cleanup();
    return true;
}

static bool test_pipeline_modes(void) {
    g_tests_run++;
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}

static bool test_mix_algos(void) {
    g_tests_run++;
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
    if(r1.total_procs!=2 || r2.total_procs!=2) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_mix_algos => mismatch => r1procs=%llu, r2procs=%llu",
                 r1.total_procs, r2.total_procs);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

static bool test_double_hpc(void) {
    g_tests_run++;
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

    if(r1.total_procs!=0 || r2.total_procs!=0) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_double_hpc => mismatch => r1=%llu, r2=%llu",
                 r1.total_procs, r2.total_procs);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

static bool test_mlfq_check(void) {
    g_tests_run++;
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

    if(r.total_procs!=3 || r.preemptions<1) {
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_mlfq_check => mismatch => procs=%llu, preempts=%llu",
                 r.total_procs, r.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_MLFQ);
    return true;
}

/* array of all modes tests */
typedef bool (*test_fn)(void);
static struct {
    const char* name;
    test_fn func;
} modes_tests[] = {
    {"hpc_over",        test_hpc_over},
    {"multi_containers",test_multi_containers},
    {"multi_distrib",   test_multi_distrib},
    {"pipeline_modes",  test_pipeline_modes},
    {"mix_algos",       test_mix_algos},
    {"double_hpc",      test_double_hpc},
    {"mlfq_check",      test_mlfq_check}
};
static const int MODES_COUNT = sizeof(modes_tests)/sizeof(modes_tests[0]);

int modes_test_count(void){ return MODES_COUNT; }
const char* modes_test_name(int i){
    if(i<0 || i>=MODES_COUNT) return NULL;
    return modes_tests[i].name;
}
void modes_test_run_single(int i, int* pass_out){
    if(!pass_out) return;
    if(i<0 || i>=MODES_COUNT) {
        *pass_out=0;
        return;
    }
    g_tests_run=0;
    g_tests_failed=0;
    bool ok = modes_tests[i].func();
    *pass_out = (ok && g_tests_failed==0) ? 1 : 0;
}

void run_modes_tests(int* total, int* passed) {
    g_tests_run=0;
    g_tests_failed=0;

    printf("\n\033[1m\033[93m╔══════════ MODES TESTS START ══════════╗\033[0m\n");
    for(int i=0; i<MODES_COUNT; i++){
        bool ok = modes_tests[i].func();
        if(ok){
            printf("  PASS: %s\n", modes_tests[i].name);
        } else {
            printf("  FAIL: %s => %s\n", modes_tests[i].name, test_get_fail_reason());
        }
    }

    *total  = g_tests_run;
    *passed = (g_tests_run - g_tests_failed);

    printf("\033[1m\033[93m╔══════════════════════════════════════════════╗\n");
    printf("║       MODES TESTS RESULTS: %d / %d passed       ║\n", *passed, *total);
    if(*passed < *total){
        printf("║    FAILURES => see logs above               ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
}
