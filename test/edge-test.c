#include "edge-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

static int tests_run=0, tests_failed=0;
extern char g_test_fail_reason[256];

static bool test_extreme_long_impl(void){
    os_init();
    process_t p[1];
    init_process(&p[0], 50, 2, 0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    /* single proc => wait=0, tat=50, resp=0, pre=0 */
    if(rep.total_procs!=1) return false;
    if(rep.preemptions!=0) return false;
    if(fabs(rep.avg_wait - 0.0)>0.001) return false;
    if(fabs(rep.avg_turnaround - 50.0)>0.1) return false;
    if(fabs(rep.avg_response - 0.0)>0.001) return false;
    return true;
}
TEST(test_extreme_long){
    if(!test_extreme_long_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_extreme_long => mismatch => expected wait=0, tat=50, resp=0");
        return false;
    }
    return true;
}

static bool test_extreme_short_impl(void){
    os_init();
    process_t p[1];
    init_process(&p[0],1,2,0);

    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    /* single => wait=0, tat=1, resp=0 => pre=0 */
    if(rep.total_procs!=1) return false;
    if(rep.preemptions!=0) return false;
    if(fabs(rep.avg_wait - 0.0)>0.001) return false;
    if(fabs(rep.avg_turnaround -1.0)>0.001) return false;
    return true;
}
TEST(test_extreme_short){
    if(!test_extreme_short_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_extreme_short => mismatch => expected wait=0, tat=1, resp=0");
        return false;
    }
    return true;
}

static bool test_high_load_impl(void){
    os_init();
    process_t arr[10];
    for(int i=0;i<10;i++){
        init_process(&arr[i], 3+(i%3), 1, 0);
    }
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(arr,10);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=10) return false;
    return true;
}
TEST(test_high_load){
    if(!test_high_load_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_high_load => mismatch => expected total_procs=10");
        return false;
    }
    return true;
}

static bool test_hpc_under_load_impl(void){
    os_init();
    os_run_hpc_overshadow();
    os_cleanup();
    return true;
}
TEST(test_hpc_under_load){
    if(!test_hpc_under_load_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_hpc_under_load => fail ???");
        return false;
    }
    return true;
}

static bool test_container_spam_impl(void){
    os_init();
    for(int i=0;i<3;i++){
        os_create_ephemeral_container();
    }
    for(int i=0;i<3;i++){
        os_remove_ephemeral_container();
    }
    os_cleanup();
    return true;
}
TEST(test_container_spam){
    if(!test_container_spam_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_container_spam => fail ???");
        return false;
    }
    return true;
}

static bool test_pipeline_edge_impl(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}
TEST(test_pipeline_edge){
    if(!test_pipeline_edge_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_pipeline_edge => fail ???");
        return false;
    }
    return true;
}

static bool test_multi_distrib_impl(void){
    os_init();
    for(int i=0;i<3;i++){
        os_run_distributed_example();
    }
    os_cleanup();
    return true;
}
TEST(test_multi_distrib){
    if(!test_multi_distrib_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_multi_distrib => fail ???");
        return false;
    }
    return true;
}

void run_edge_tests(int* total,int* passed){
    tests_run=0;
    tests_failed=0;

    RUN_TEST(test_extreme_long);
    RUN_TEST(test_extreme_short);
    RUN_TEST(test_high_load);
    RUN_TEST(test_hpc_under_load);
    RUN_TEST(test_container_spam);
    RUN_TEST(test_pipeline_edge);
    RUN_TEST(test_multi_distrib);

    *total=tests_run;
    *passed=tests_run - tests_failed;
}
