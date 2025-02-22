#include "normal-test.h"
#include "test_common.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include <string.h>

static int tests_run=0, tests_failed=0;

static void sc_sjf(void){
    os_init();
    process_t p[3];
    init_process(&p[0],1,1,os_time());
    init_process(&p[1],5,1,os_time());
    init_process(&p[2],2,1,os_time());
    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,3);
    os_cleanup();
}
static void sc_strf(void){
    os_init();
    process_t p[2];
    init_process(&p[0],4,1,os_time());
    init_process(&p[1],3,1,os_time());
    scheduler_select_algorithm(ALG_STRF);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_hrrn(void){
    os_init();
    process_t p[3];
    for(int i=0; i<3; i++){
        init_process(&p[i], 2+i, 1, os_time());
    }
    scheduler_select_algorithm(ALG_HRRN);
    scheduler_run(p,3);
    os_cleanup();
}
static void sc_hrrn_rt(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,1,os_time());
    init_process(&p[1],4,2,os_time());
    scheduler_select_algorithm(ALG_HRRN_RT);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_prio(void){
    os_init();
    process_t p[3];
    init_process(&p[0],2,3,os_time());
    init_process(&p[1],2,1,os_time());
    init_process(&p[2],2,2,os_time());
    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,3);
    os_cleanup();
}
static void sc_cfs_srtf(void){
    os_init();
    process_t p[3];
    for(int i=0; i<3; i++){
        init_process(&p[i], 2+i, 1, os_time());
    }
    scheduler_select_algorithm(ALG_CFS_SRTF);
    scheduler_run(p, 3);
    os_cleanup();
}

TEST(test_sjf){
    struct captured_output cap;
    int st=run_function_capture_output(sc_sjf, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Stats for SJF")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_strf){
    struct captured_output cap;
    int st=run_function_capture_output(sc_strf, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Stats for STRF")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_hrrn){
    struct captured_output cap;
    int st=run_function_capture_output(sc_hrrn, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Stats for HRRN")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_hrrn_rt){
    struct captured_output cap;
    int st=run_function_capture_output(sc_hrrn_rt, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Stats for HRRN-RT")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_priority){
    struct captured_output cap;
    int st=run_function_capture_output(sc_prio, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Stats for PRIORITY")
            && strstr(cap.stdout_buf,"Cleanup"));
}
TEST(test_cfs_srtf){
    struct captured_output cap;
    int st=run_function_capture_output(sc_cfs_srtf, &cap);
    return (st==0 && strstr(cap.stdout_buf,"Init")
            && strstr(cap.stdout_buf,"Stats for CFS-SRTF")
            && strstr(cap.stdout_buf,"Cleanup"));
}

void run_normal_tests(int* total, int* passed){
    tests_run=0; tests_failed=0;

    RUN_TEST(test_sjf);
    RUN_TEST(test_strf);
    RUN_TEST(test_hrrn);
    RUN_TEST(test_hrrn_rt);
    RUN_TEST(test_priority);
    RUN_TEST(test_cfs_srtf);

    *total  = tests_run;
    *passed = (tests_run - tests_failed);
}
