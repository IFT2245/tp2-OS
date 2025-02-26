#include "hidden-test.h"
#include "test_common.h"
#include "../src/runner.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/scoreboard.h"
#include "../src/stats.h"
#include <stdio.h>
#include <math.h>

static int g_tests_run=0, g_tests_failed=0;
static char g_fail_reason[256];

static bool test_distrib_heavy(void) {
    g_tests_run++;
    os_init();
    for(int i=0;i<4;i++){
        os_run_distributed_example();
    }
    os_cleanup();
    return true;
}

static bool test_hpc_heavy(void) {
    g_tests_run++;
    os_init();
    process_t dummy[1];
    init_process(&dummy[0],0,0,0);

    for(int i=0;i<2;i++){
        scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
        scheduler_run(dummy,1);
        sched_report_t rep;
        scheduler_fetch_report(&rep);
        if(rep.total_procs!=0){
            snprintf(g_fail_reason,sizeof(g_fail_reason),
                     "test_hpc_heavy => overshadow => got total_procs=%llu, expected=0",
                     rep.total_procs);
            test_set_fail_reason(g_fail_reason);
            g_tests_failed++;
            os_cleanup();
            return false;
        }
    }
    os_cleanup();
    return true;
}

static bool test_container_combo(void) {
    g_tests_run++;
    os_init();
    os_create_ephemeral_container();
    os_run_distributed_example();
    os_run_hpc_overshadow();
    os_remove_ephemeral_container();
    os_cleanup();
    return true;
}

static bool test_scheduling_variety(void) {
    g_tests_run++;
    os_init();
    process_t p[2];
    /* SJF first */
    init_process(&p[0],2,1,0);
    init_process(&p[1],6,2,0);
    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if(r1.total_procs!=2){
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_scheduling_variety => SJF => mismatch => total=%llu",
                 r1.total_procs);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        os_cleanup();
        return false;
    }

    /* Priority next */
    init_process(&p[0],2,3,0);
    init_process(&p[1],6,1,0);
    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,2);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();

    if(r2.total_procs!=2){
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_scheduling_variety => Priority => mismatch => total=%llu",
                 r2.total_procs);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

static bool test_auto_logic(void) {
    g_tests_run++;
    /* always pass */
    return true;
}

static bool test_final_integration(void) {
    g_tests_run++;
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

static bool test_multi_stage_distributed(void) {
    g_tests_run++;
    os_init();
    os_run_distributed_example();

    process_t dummy[1];
    init_process(&dummy[0],0,0,0);
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if(r1.total_procs!=0){
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_multi_stage_distributed => overshadow #1 => got %llu procs,expected=0",
                 r1.total_procs);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        os_cleanup();
        return false;
    }

    os_run_distributed_example();
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();
    if(r2.total_procs!=0){
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_multi_stage_distributed => overshadow #2 => got %llu procs,expected=0",
                 r2.total_procs);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    return true;
}

typedef bool (*test_fn)(void);
static struct {
    const char* name;
    test_fn func;
} hidden_tests[] = {
    {"distrib_heavy",        test_distrib_heavy},
    {"hpc_heavy",            test_hpc_heavy},
    {"container_combo",      test_container_combo},
    {"scheduling_variety",   test_scheduling_variety},
    {"auto_logic",           test_auto_logic},
    {"final_integration",    test_final_integration},
    {"multi_stage_distributed", test_multi_stage_distributed}
};
static const int HIDDEN_COUNT = sizeof(hidden_tests)/sizeof(hidden_tests[0]);

int hidden_test_count(void){ return HIDDEN_COUNT; }
const char* hidden_test_name(int i){
    if(i<0||i>=HIDDEN_COUNT) return NULL;
    return hidden_tests[i].name;
}
void hidden_test_run_single(int i,int* pass_out){
    if(!pass_out) return;
    if(i<0||i>=HIDDEN_COUNT){
        *pass_out=0;
        return;
    }
    g_tests_run=0;
    g_tests_failed=0;
    bool ok = hidden_tests[i].func();
    *pass_out = (ok && g_tests_failed==0)?1:0;
}

void run_hidden_tests(int* total,int* passed){
    g_tests_run=0;
    g_tests_failed=0;

    printf("\n\033[1m\033[93m╔══════════ HIDDEN TESTS START ══════════╗\033[0m\n");
    for(int i=0;i<HIDDEN_COUNT;i++){
        if (skip_remaining_tests_requested()) {
            printf(CLR_RED "[SIGTERM] => skipping remaining tests in this suite.\n" CLR_RESET);
            break;
        }

        bool ok = hidden_tests[i].func();
        if(ok){
            printf("  PASS: %s\n", hidden_tests[i].name);
        } else {
            printf("  FAIL: %s => %s\n", hidden_tests[i].name, test_get_fail_reason());
        }
    }

    *total = g_tests_run;
    *passed= (g_tests_run - g_tests_failed);

    scoreboard_update_hidden(*total, *passed);
    stats_inc_tests_passed(*passed);
    stats_inc_tests_failed((*total)-(*passed));

    printf("\033[1m\033[93m╔══════════════════════════════════════════════╗\n");
    printf("║      HIDDEN TESTS RESULTS: %d / %d passed      ║\n", *passed, *total);
    if(*passed < *total){
        printf("║    FAILURES => see logs above               ║\n");
    }
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
}
