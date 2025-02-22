#include "basic-test.h"
#include "test_common.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include <string.h>
#include <stdio.h>

static int tests_run=0, tests_failed=0;

/* Test wrappers */
static void sc_fifo(void){
    os_init();
    process_t p[2];
    init_process(&p[0], 3, 1, os_time());
    init_process(&p[1], 5, 1, os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p, 2);
    os_cleanup();
}
static void sc_rr(void){
    os_init();
    process_t p[2];
    init_process(&p[0], 2, 1, os_time());
    init_process(&p[1], 2, 1, os_time());
    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p, 2);
    os_cleanup();
}
static void sc_cfs(void){
    os_init();
    process_t p[2];
    init_process(&p[0], 3, 0, os_time());
    init_process(&p[1], 4, 0, os_time());
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(p, 2);
    os_cleanup();
}
static void sc_bfs(void){
    os_init();
    process_t p[3];
    for(int i=0; i<3; i++){
        init_process(&p[i], 2+i, 0, os_time());
    }
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p, 3);
    os_cleanup();
}
static void sc_pipeline(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
}
static void sc_distributed(void){
    os_init();
    os_run_distributed_example();
    os_cleanup();
}

/* Tests */
TEST(test_fifo){
    struct captured_output cap;
    int st = run_function_capture_output(sc_fifo, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Init") && strstr(cap.stdout_buf, "Stats for FIFO") && strstr(cap.stdout_buf, "Cleanup"));
    return pass;
}
TEST(test_rr){
    struct captured_output cap;
    int st = run_function_capture_output(sc_rr, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Init") && strstr(cap.stdout_buf, "Stats for RR") && strstr(cap.stdout_buf, "Cleanup"));
    return pass;
}
TEST(test_cfs){
    struct captured_output cap;
    int st = run_function_capture_output(sc_cfs, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Init") && strstr(cap.stdout_buf, "Stats for CFS") && strstr(cap.stdout_buf, "Cleanup"));
    return pass;
}
TEST(test_bfs){
    struct captured_output cap;
    int st = run_function_capture_output(sc_bfs, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Init") && strstr(cap.stdout_buf, "Stats for BFS") && strstr(cap.stdout_buf, "Cleanup"));
    return pass;
}
TEST(test_pipeline){
    struct captured_output cap;
    int st = run_function_capture_output(sc_pipeline, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Init") && strstr(cap.stdout_buf, "Pipeline start")
                 && strstr(cap.stdout_buf, "Pipeline end") && strstr(cap.stdout_buf, "Cleanup"));
    return pass;
}
TEST(test_distributed){
    struct captured_output cap;
    int st = run_function_capture_output(sc_distributed, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Init")
                 && strstr(cap.stdout_buf, "Distributed example: fork")
                 && strstr(cap.stdout_buf, "HPC overshadow done")
                 && strstr(cap.stdout_buf, "Cleanup"));
    return pass;
}

/* Entry point */
void run_basic_tests(int* total, int* passed){
    tests_run   = 0;
    tests_failed= 0;

    RUN_TEST(test_fifo);
    RUN_TEST(test_rr);
    RUN_TEST(test_cfs);
    RUN_TEST(test_bfs);
    RUN_TEST(test_pipeline);
    RUN_TEST(test_distributed);

    *total  = tests_run;
    *passed = (tests_run - tests_failed);
}
