#include "hidden-test.h"
#include "test_common.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/scoreboard.h"
#include <stdio.h>
#include <math.h>

static int tests_run=0, tests_failed=0;
static char g_test_fail_reason[256];

/* prototypes */
static bool test_distrib_heavy(void);
static bool test_hpc_heavy(void);
static bool test_container_combo(void);
static bool test_scheduling_variety(void);
static bool test_auto_logic(void);
static bool test_final_integration(void);
static bool test_multi_stage_distributed(void);

/* For sub-sub menu. */
typedef bool(*hidden_test_fn)(void);
static hidden_test_fn g_hidden_tests[] = {
    test_distrib_heavy,
    test_hpc_heavy,
    test_container_combo,
    test_scheduling_variety,
    test_auto_logic,
    test_final_integration,
    test_multi_stage_distributed
};
static const char* g_hidden_test_names[] = {
    "Distrib Heavy",
    "HPC Heavy",
    "Container + HPC Combo",
    "Scheduling Variety",
    "Auto Logic (dummy)",
    "Final Integration",
    "Multi-stage Distributed"
};
static const int g_hidden_count = (int)(sizeof(g_hidden_tests)/sizeof(g_hidden_tests[0]));

int hidden_test_get_count(void){ return g_hidden_count; }
const char* hidden_test_get_name(int index){
    if(index<0 || index>=g_hidden_count) return NULL;
    return g_hidden_test_names[index];
}
int hidden_test_run_single(int index){
    if(index<0 || index>=g_hidden_count) return 0;
    tests_run=0; tests_failed=0;
    bool ok = g_hidden_tests[index]();
    int passcount = ok ? 1 : 0;
    scoreboard_update_hidden(1, passcount);
    scoreboard_save();
    return passcount;
}

/* Helpers */
static int almost_equal(double a, double b, double eps){
    return fabs(a - b) < eps;
}

static bool test_distrib_heavy(void){
    os_init();
    tests_run++;
    for(int i=0;i<4;i++){
        os_run_distributed_example();
    }
    os_cleanup();
    return true;
}

static bool test_hpc_heavy(void){
    os_init();
    tests_run++;
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    for(int i=0;i<2;i++){
        scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
        scheduler_run(dummy,1);
        sched_report_t rep;
        scheduler_fetch_report(&rep);
        if(rep.total_procs != 0){
            tests_failed++;
            char buf[256];
            snprintf(buf,sizeof(buf),
                     "test_hpc_heavy => overshadow => expected total_procs=0, got %llu",
                     rep.total_procs);
            os_cleanup();
            return false;
        }
    }
    os_cleanup();
    return true;
}

static bool test_container_combo(void){
    os_init();
    tests_run++;
    os_create_ephemeral_container();
    os_run_distributed_example();
    os_run_hpc_overshadow();
    os_remove_ephemeral_container();
    os_cleanup();
    return true;
}

static bool test_scheduling_variety(void){
    os_init();
    tests_run++;

    process_t p[2];
    init_process(&p[0],2,1,0);
    init_process(&p[1],6,2,0);

    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if(r1.total_procs!=2 || r1.preemptions!=0ULL){
        tests_failed++;
        char buf[256];
        snprintf(buf,sizeof(buf),
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

    if(r2.total_procs!=2 || r2.preemptions!=0ULL){
        tests_failed++;
        char buf[256];
        snprintf(buf,sizeof(buf),
                 "test_scheduling_variety => Priority => mismatch => total=%llu, pre=%llu",
                 r2.total_procs, r2.preemptions);
        return false;
    }

    return true;
}

static bool test_auto_logic(void){
    tests_run++;
    /* no real auto => pass */
    return true;
}

static bool test_final_integration(void){
    os_init();
    tests_run++;
    os_log("Final synergy HPC + container + pipeline + distributed");
    os_create_ephemeral_container();
    os_run_hpc_overshadow();
    os_run_distributed_example();
    os_pipeline_example();
    os_remove_ephemeral_container();
    os_cleanup();
    return true;
}

static bool test_multi_stage_distributed(void){
    os_init();

    tests_run++;
    os_run_distributed_example();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if(r1.total_procs != 0){
        tests_failed++;
        char buf[256];
        snprintf(buf,sizeof(buf),
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

    if(r2.total_procs != 0){
        tests_failed++;
        char buf[256];
        snprintf(buf,sizeof(buf),
                 "test_multi_stage_distributed => overshadow #2 => expected 0 procs, got %llu",
                 r2.total_procs);
        return false;
    }
    return true;
}

void run_hidden_tests(int* total,int* passed){
    tests_run=0;
    tests_failed=0;

    test_distrib_heavy();
    test_hpc_heavy();
    test_container_combo();
    test_scheduling_variety();
    test_auto_logic();
    test_final_integration();
    test_multi_stage_distributed();

    *total = tests_run;
    *passed= (tests_run - tests_failed);
}
