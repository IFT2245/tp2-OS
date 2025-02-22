#include "basic-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/scoreboard.h"

static int tests_run=0,tests_failed=0;

static void sc_fifo_run(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,1,os_time());
    init_process(&p[1],5,1,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_rr_run(void){
    os_init();
    process_t p[2];
    init_process(&p[0],2,1,os_time());
    init_process(&p[1],2,1,os_time());
    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_cfs_run(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,0,os_time());
    init_process(&p[1],4,0,os_time());
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_bfs_run(void){
    os_init();
    process_t p[3];
    for(int i=0;i<3;i++){
        init_process(&p[i],2+i,0,os_time());
    }
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,3);
    os_cleanup();
}
static void sc_pipeline_run(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
}
static void sc_distributed_run(void){
    os_init();
    os_run_distributed_example();
    os_cleanup();
}

TEST(test_fifo){
    struct captured_output cap;
    int st=run_function_capture_output(sc_fifo_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for FIFO"));
    if(pass) scoreboard_set_sc_mastered(ALG_FIFO);
    return pass;
}
TEST(test_rr){
    struct captured_output cap;
    int st=run_function_capture_output(sc_rr_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for RR"));
    if(pass) scoreboard_set_sc_mastered(ALG_RR);
    return pass;
}
TEST(test_cfs){
    struct captured_output cap;
    int st=run_function_capture_output(sc_cfs_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for CFS"));
    if(pass) scoreboard_set_sc_mastered(ALG_CFS);
    return pass;
}
TEST(test_bfs){
    struct captured_output cap;
    int st=run_function_capture_output(sc_bfs_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for BFS"));
    if(pass) scoreboard_set_sc_mastered(ALG_BFS);
    return pass;
}
TEST(test_pipeline){
    struct captured_output cap;
    int st=run_function_capture_output(sc_pipeline_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Pipeline start") && strstr(cap.stdout_buf,"Pipeline end"));
    return pass;
}
TEST(test_distributed){
    struct captured_output cap;
    int st=run_function_capture_output(sc_distributed_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Distributed example: fork"));
    return pass;
}

void run_basic_tests(int* total,int* passed){
    tests_run=0; tests_failed=0;
    RUN_TEST(test_fifo);
    RUN_TEST(test_rr);
    RUN_TEST(test_cfs);
    RUN_TEST(test_bfs);
    RUN_TEST(test_pipeline);
    RUN_TEST(test_distributed);
    *total=tests_run;
    *passed=(tests_run - tests_failed);
}
