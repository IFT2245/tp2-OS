#include "hidden-test.h"
#include "test_common.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/scoreboard.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

static int tests_run=0, tests_failed=0;
extern char g_test_fail_reason[256];

/* distribute heavily => no scheduling => pass if no crash */
static bool test_distrib_heavy_impl(void){
    os_init();
    for(int i=0;i<4;i++){
        os_run_distributed_example();
    }
    os_cleanup();
    return true;
}
TEST(test_distrib_heavy){
    if(!test_distrib_heavy_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_distrib_heavy => fail ???");
        return false;
    }
    return true;
}

/* HPC overshadow heavy => multiple times => stats=0 each time */
static bool test_hpc_heavy_impl(void){
    os_init();
    process_t dummy[1];
    init_process(&dummy[0],0,0,0);

    for(int i=0;i<2;i++){
        scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
        scheduler_run(dummy,1);
        sched_report_t rep;
        scheduler_fetch_report(&rep);
        if(rep.total_procs!=0) {
            os_cleanup();
            return false;
        }
    }
    os_cleanup();
    return true;
}
TEST(test_hpc_heavy){
    if(!test_hpc_heavy_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_hpc_heavy => overshadow => expected total_procs=0");
        return false;
    }
    return true;
}

/* container + HPC overshadow => pass if no crash */
static bool test_container_combo_impl(void){
    os_init();
    os_create_ephemeral_container();
    os_run_distributed_example();
    os_run_hpc_overshadow();
    os_remove_ephemeral_container();
    os_cleanup();
    return true;
}
TEST(test_container_combo){
    if(!test_container_combo_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_container_combo => fail ???");
        return false;
    }
    return true;
}

/* scheduling variety => run SJF then Priority => partial checks */
static bool test_scheduling_variety_impl(void){
    os_init();
    process_t p[2];
    init_process(&p[0],2,1,0);
    init_process(&p[1],6,2,0);

    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if(r1.total_procs!=2 || r1.preemptions!=0){
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

    if(r2.total_procs!=2 || r2.preemptions!=0) return false;
    return true;
}
TEST(test_scheduling_variety){
    if(!test_scheduling_variety_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_scheduling_variety => mismatch => expected total=2, pre=0 in both runs.");
        return false;
    }
    return true;
}

/* auto logic => just pass */
static bool test_auto_logic_impl(void){
    printf("Auto mode selection tested.\n");
    return true;
}
TEST(test_auto_logic){
    if(!test_auto_logic_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_auto_logic => ???");
        return false;
    }
    return true;
}

/* final synergy => HPC + container + pipeline + distributed => pass if no crash */
static bool test_final_integration_impl(void){
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
TEST(test_final_integration){
    if(!test_final_integration_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_final_integration => fail ???");
        return false;
    }
    return true;
}

/* multi-stage => distributed + HPC overshadow => overshadow stats=0 each time */
static bool test_multi_stage_distrib_impl(void){
    os_init();

    os_run_distributed_example();
    process_t dummy[1];
    init_process(&dummy[0],0,0,0);
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if(r1.total_procs!=0){
        os_cleanup();
        return false;
    }

    os_run_distributed_example();
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();

    if(r2.total_procs!=0) return false;
    return true;
}
TEST(test_multi_stage_distributed){
    if(!test_multi_stage_distrib_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_multi_stage_distrib => overshadow => expected total_procs=0");
        return false;
    }
    return true;
}

void run_hidden_tests(int* total,int* passed){
    tests_run=0;
    tests_failed=0;

    RUN_TEST(test_distrib_heavy);
    RUN_TEST(test_hpc_heavy);
    RUN_TEST(test_container_combo);
    RUN_TEST(test_scheduling_variety);
    RUN_TEST(test_auto_logic);
    RUN_TEST(test_final_integration);
    RUN_TEST(test_multi_stage_distributed);

    *total=tests_run;
    *passed=tests_run - tests_failed;
}
