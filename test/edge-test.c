#include "edge-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"

static int tests_run=0, tests_failed=0;

static void sc_extreme_long(void){
    os_init();
    process_t p[1];
    init_process(&p[0],50,2,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,1);
    os_cleanup();
}
static void sc_extreme_short(void){
    os_init();
    process_t p[1];
    init_process(&p[0],1,2,os_time());
    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,1);
    os_cleanup();
}
static void sc_high_load(void){
    os_init();
    process_t arr[10];
    for(int i=0;i<10;i++){
        init_process(&arr[i],3+(i%3),1,os_time());
    }
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(arr,10);
    os_cleanup();
}
static void sc_hpc_under_load(void){
    os_init();
    os_run_hpc_overshadow();
    os_cleanup();
}
static void sc_container_spam(void){
    os_init();
    for(int i=0;i<3;i++) os_create_ephemeral_container();
    for(int i=0;i<3;i++) os_remove_ephemeral_container();
    os_cleanup();
}
static void sc_pipeline_edge(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
}
static void sc_multi_distributed(void){
    os_init();
    for(int i=0;i<3;i++){
        os_run_distributed_example();
    }
    os_cleanup();
}

TEST(test_extreme_long_burst){
    struct captured_output cap;
    int st = run_function_capture_output(sc_extreme_long, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for FIFO"));
    return pass;
}
TEST(test_extreme_short_burst){
    struct captured_output cap;
    int st = run_function_capture_output(sc_extreme_short, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for RR"));
    return pass;
}
TEST(test_high_load){
    struct captured_output cap;
    int st = run_function_capture_output(sc_high_load, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for CFS"));
    return pass;
}
TEST(test_hpc_under_load){
    struct captured_output cap;
    int st = run_function_capture_output(sc_hpc_under_load, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "HPC overshadow start"));
    return pass;
}
TEST(test_container_spam){
    struct captured_output cap;
    int st = run_function_capture_output(sc_container_spam, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Container created"));
    return pass;
}
TEST(test_pipeline_edge){
    struct captured_output cap;
    int st = run_function_capture_output(sc_pipeline_edge, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Pipeline start"));
    return pass;
}
TEST(test_multi_distributed){
    struct captured_output cap;
    int st = run_function_capture_output(sc_multi_distributed, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Distributed example: fork"));
    return pass;
}

void run_edge_tests(int* total,int* passed){
    tests_run=0; tests_failed=0;
    RUN_TEST(test_extreme_long_burst);
    RUN_TEST(test_extreme_short_burst);
    RUN_TEST(test_high_load);
    RUN_TEST(test_hpc_under_load);
    RUN_TEST(test_container_spam);
    RUN_TEST(test_pipeline_edge);
    RUN_TEST(test_multi_distributed);
    *total  = tests_run;
    *passed = (tests_run - tests_failed);
}
