#include "modes-test.h"
#include "test_common.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/process.h"
#include <string.h>

static int tests_run=0, tests_failed=0;

static void sc_hpc_over(void){
    os_init();
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    process_t d[1];
    init_process(&d[0], 0, 0, os_time());
    scheduler_run(d, 1);
    os_cleanup();
}
static void sc_multi_containers(void){
    os_init();
    os_create_ephemeral_container();
    os_create_ephemeral_container();
    os_remove_ephemeral_container();
    os_remove_ephemeral_container();
    os_cleanup();
}
static void sc_multi_distrib(void){
    os_init();
    for(int i=0; i<2; i++){
        os_run_distributed_example();
    }
    os_cleanup();
}
static void sc_pipeline(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
}
static void sc_mix_algos(void){
    os_init();
    process_t p[2];
    init_process(&p[0],2,1,os_time());
    init_process(&p[1],3,1,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_double_hpc(void){
    os_init();
    os_run_hpc_overshadow();
    os_run_hpc_overshadow();
    os_cleanup();
}

TEST(test_hpc_over){
    struct captured_output cap;
    int st=run_function_capture_output(sc_hpc_over, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Stats for HPC-OVER")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_multi_containers){
    struct captured_output cap;
    int st=run_function_capture_output(sc_multi_containers, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Container created")
            && strstr(cap.stdout_buf,"Container removed")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_multi_distrib){
    struct captured_output cap;
    int st=run_function_capture_output(sc_multi_distrib, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Distributed example: fork")
            && strstr(cap.stdout_buf,"HPC overshadow done")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_pipeline_modes){
    struct captured_output cap;
    int st=run_function_capture_output(sc_pipeline, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Pipeline start")
            && strstr(cap.stdout_buf,"Pipeline end")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_mix_algos){
    struct captured_output cap;
    int st=run_function_capture_output(sc_mix_algos, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Stats for FIFO")
            && strstr(cap.stdout_buf,"Stats for BFS")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_double_hpc){
    struct captured_output cap;
    int st=run_function_capture_output(sc_double_hpc, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"HPC overshadow start")
            && strstr(cap.stdout_buf,"HPC overshadow done")
            && strstr(cap.stdout_buf,"Cleanup"));
}

void run_modes_tests(int* total, int* passed){
    tests_run=0; tests_failed=0;

    RUN_TEST(test_hpc_over);
    RUN_TEST(test_multi_containers);
    RUN_TEST(test_multi_distrib);
    RUN_TEST(test_pipeline_modes);
    RUN_TEST(test_mix_algos);
    RUN_TEST(test_double_hpc);

    *total  = tests_run;
    *passed = (tests_run - tests_failed);
}
