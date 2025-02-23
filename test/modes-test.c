#include "modes-test.h"
#include "test_common.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

static int tests_run=0, tests_failed=0;
extern char g_test_fail_reason[256];

static bool test_hpc_over_impl(void){
    os_init();
    process_t dummy[1];
    init_process(&dummy[0],0,0,0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=0) return false;
    if(rep.preemptions!=0) return false;
    if(rep.avg_wait!=0.0) return false;
    return true;
}
TEST(test_hpc_over){
    if(!test_hpc_over_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_hpc_over => HPC overshadow => expected 0 stats");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HPC_OVERSHADOW);
    return true;
}

static bool test_multi_containers_impl(void){
    os_init();
    for(int i=0;i<2;i++) os_create_ephemeral_container();
    for(int i=0;i<2;i++) os_remove_ephemeral_container();
    os_cleanup();
    return true;
}
TEST(test_multi_containers){
    if(!test_multi_containers_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_multi_containers => fail ???");
        return false;
    }
    return true;
}

static bool test_multi_distrib_impl(void){
    os_init();
    os_run_distributed_example();
    os_run_distributed_example();
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

static bool test_pipeline_modes_impl(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}
TEST(test_pipeline_modes){
    if(!test_pipeline_modes_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_pipeline_modes => fail ???");
        return false;
    }
    return true;
}

static bool test_mix_algos_impl(void){
    os_init();
    /* first FIFO => 2 procs => no preempt => total=2, preempt=0 */
    process_t p[2];
    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if(r1.total_procs!=2 || r1.preemptions!=0){
        os_cleanup();
        return false;
    }

    /* BFS => reinit => partial => expect preempt>0. */
    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,2);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();

    if(r2.total_procs!=2 || r2.preemptions<1) return false;
    return true;
}
TEST(test_mix_algos){
    if(!test_mix_algos_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_mix_algos => mismatch in FIFO or BFS portion");
        return false;
    }
    return true;
}

static bool test_double_hpc_impl(void){
    os_init();
    process_t dummy[1];
    init_process(&dummy[0],0,0,0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r1;
    scheduler_fetch_report(&r1);
    if(r1.total_procs!=0) {os_cleanup();return false;}

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);
    sched_report_t r2;
    scheduler_fetch_report(&r2);
    os_cleanup();

    if(r2.total_procs!=0) return false;
    return true;
}
TEST(test_double_hpc){
    if(!test_double_hpc_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_double_hpc => overshadow => expected 0 stats each time");
        return false;
    }
    return true;
}

static bool test_mlfq_check_impl(void){
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

    /* we just check total=3, preempt>0 */
    if(r.total_procs!=3) return false;
    if(r.preemptions<1)  return false;
    scoreboard_set_sc_mastered(ALG_MLFQ);
    return true;
}
TEST(test_mlfq_check){
    if(!test_mlfq_check_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_mlfq_check => mismatch => expected total=3, preempt>0");
        return false;
    }
    return true;
}

void run_modes_tests(int* total,int* passed){
    tests_run=0;
    tests_failed=0;

    RUN_TEST(test_hpc_over);
    RUN_TEST(test_multi_containers);
    RUN_TEST(test_multi_distrib);
    RUN_TEST(test_pipeline_modes);
    RUN_TEST(test_mix_algos);
    RUN_TEST(test_double_hpc);
    RUN_TEST(test_mlfq_check);

    *total=tests_run;
    *passed=tests_run - tests_failed;
}
